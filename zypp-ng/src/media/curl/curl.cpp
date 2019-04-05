#include <media/curl/private/curl_p.h>
#include <media/curl/private/request_p.h>
#include <boost/asio/io_context.hpp>
#include <boost/date_time/time.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/placeholders.hpp>
#include <iostream>
#include <assert.h>

#include <zypp/base/String.h>

using namespace boost;

namespace zyppng {


CurlSocketListener::CurlSocketListener( asio::io_context &ctx, curl_socket_t s, CurlSocketListener::Mode waitMode )
    : _socket( s )
{
  _strDesc = std::make_unique<boost::asio::posix::stream_descriptor>( ctx, s );
  setMode( waitMode );
}

CurlSocketListener::~CurlSocketListener()
{
  //we do not own the descriptor, release it
  _strDesc->release();
}

void CurlSocketListener::setMode( int mode )
{
  auto doReg = [ this, mode ]( std::shared_ptr<CurlSocketListener::Handler> &ptr, Mode tryMode, asio::posix::stream_descriptor::wait_type asioType ){
    if ( mode & tryMode ) {
      if ( !ptr ) {
        ptr = std::make_shared<Handler>();
        ptr->enabled = true;
        ptr->parent = this;
        ptr->mode = tryMode;
        _strDesc->async_wait( asioType, boost::bind( &Handler::staticCallback, ptr, boost::asio::placeholders::error ) );
      }
    } else {
      if ( ptr ) {
        ptr->enabled = false;
        //release the pointer here, all possibly still running handler will have their own instance of it
        ptr.reset();
      }
    }
  };

  doReg( _readHandler, Mode::WaitRead, asio::posix::stream_descriptor::wait_read );
  doReg( _writeHandler, Mode::WaitWrite, asio::posix::stream_descriptor::wait_write );
  doReg( _errHandler, Mode::WaitErr, asio::posix::stream_descriptor::wait_error );
}

curl_socket_t CurlSocketListener::nativeHandle() const
{
  return _strDesc->native_handle();
}

void CurlSocketListener::Handler::staticCallback(std::shared_ptr<CurlSocketListener::Handler> that, const system::error_code &ec)
{
  if ( !that )
    return;
  return that->handler( that, ec );
}

void CurlSocketListener::Handler::handler( std::shared_ptr<CurlSocketListener::Handler> that,  const boost::system::error_code& ec )
{
  if ( ec == asio::error::operation_aborted )
    return;

  if ( !enabled )
    return;

  assert( this == that.get() );

  if ( !ec && mode == Mode::WaitRead ) {
    parent->_readyRead.emit( *parent );
    if ( enabled )
      parent->_strDesc->async_wait( asio::posix::stream_descriptor::wait_write, boost::bind( &Handler::staticCallback, that, boost::asio::placeholders::error) );
  } else if ( !ec && mode == Mode::WaitWrite ) {
    parent->_readyWrite.emit( *parent );
    if ( enabled )
      parent->_strDesc->async_wait( asio::posix::stream_descriptor::wait_write, boost::bind( &Handler::staticCallback, that, boost::asio::placeholders::error) );
  } else if ( ec || mode == Mode::WaitErr ) {
    parent->_error.emit( *parent );
    if ( enabled )
      parent->_strDesc->async_wait( asio::posix::stream_descriptor::wait_error, boost::bind( &Handler::staticCallback, that, boost::asio::placeholders::error) );
  }
}

SignalProxy<void (const CurlSocketListener &)> CurlSocketListener::sigReadyRead()
{ return _readyRead; }

SignalProxy<void (const CurlSocketListener &)> CurlSocketListener::sigReadyWrite()
{ return _readyWrite; }

SignalProxy<void (const CurlSocketListener &)> CurlSocketListener::sigError()
{ return _error; }


HttpRequestDispatcherPrivate::HttpRequestDispatcherPrivate(std::string &&target) : _targetDir( std::move(target))
{ }

HttpRequestDispatcherPrivate::~HttpRequestDispatcherPrivate()
{ }

//called by curl to setup a timer
int HttpRequestDispatcherPrivate::multi_timer_cb( CURLM *, long timeout_ms, void *thatPtr )
{
  HttpRequestDispatcherPrivate *that = reinterpret_cast<HttpRequestDispatcherPrivate *>( thatPtr );
  assert( that != nullptr );

  //cancel the timer
  that->_timer->cancel();

  if ( timeout_ms > -1 ) {
    that->_timer->expires_from_now( posix_time::millisec(timeout_ms) );
    that->_timer->async_wait( [ that ]( const system::error_code &ec ) {
      if ( ec == asio::error::operation_aborted )
        return;
      that->handleMultiSocketAction( CURL_SOCKET_TIMEOUT, 0 );
    });
  }
  return 0;
}

int HttpRequestDispatcherPrivate::socket_callback( CURL * easy, curl_socket_t s, int what, void *userp, CurlSocketListener *socketp )
{
  HttpRequestDispatcherPrivate *that = reinterpret_cast<HttpRequestDispatcherPrivate *>( userp );
  assert( that != nullptr );

  if ( !socketp ) {
    if ( what == CURL_POLL_REMOVE || what == CURL_POLL_NONE )
      return 0;

    socketp = new CurlSocketListener( *that->_ioContext, s );
    CURLMcode rc = curl_multi_assign( that->_multi, s, socketp );

    //generally this should only happen when memory is full, still try to handle it correctly
    if( rc != CURLM_OK) {
      delete socketp;

      void *privatePtr = nullptr;
      if ( curl_easy_getinfo( easy, CURLINFO_PRIVATE, &privatePtr ) != CURLE_OK ) {
        privatePtr = nullptr; //make sure this was not filled with bad info
      }

      if ( privatePtr ) {
        HttpDownloadRequestPrivate *request = reinterpret_cast<HttpDownloadRequestPrivate *>( privatePtr );
        //we stop the download, if we can not listen for socket changes we can not correctly do anything
        that->setFinished( *request->z_func(), HttpRequestError( HttpRequestError::CurlMError, rc ) );
        return 0;
      } else {
        //a broken handle without anything assigned, also should never happen but make sure and clean it up
        std::cerr << "Received easy handle without assigned request pointer" << std::endl;
        curl_multi_remove_handle( that->_multi, easy );
        curl_easy_cleanup( easy );
        return 0;
      }
    }

    socketp->sigReadyRead().connect( sigc::mem_fun(*that, &HttpRequestDispatcherPrivate::onSocketReadyRead) );
    socketp->sigReadyWrite().connect( sigc::mem_fun(*that, &HttpRequestDispatcherPrivate::onSocketReadyWrite) );
    socketp->sigError().connect( sigc::mem_fun(*that, &HttpRequestDispatcherPrivate::onSocketError) );
  }

  //remove the socket
  if ( what == CURL_POLL_REMOVE ) {
    socketp->setMode( CurlSocketListener::Disabled );
    curl_multi_assign( that->_multi, s, nullptr );
    delete socketp;
    return 0;
  }

  if ( what == CURL_POLL_IN ) {
    socketp->setMode( CurlSocketListener::WaitRead | CurlSocketListener::WaitErr );
  } else if ( what == CURL_POLL_OUT ) {
    socketp->setMode( CurlSocketListener::WaitWrite | CurlSocketListener::WaitErr );
  } else if ( what == CURL_POLL_INOUT ) {
    socketp->setMode( CurlSocketListener::WaitRead | CurlSocketListener::WaitWrite | CurlSocketListener::WaitErr );
  }

  return 0;
}


void HttpRequestDispatcherPrivate::onSocketReadyRead(const CurlSocketListener &listener)
{
  handleMultiSocketAction( listener.nativeHandle(), CURL_CSELECT_IN );
}

void HttpRequestDispatcherPrivate::onSocketReadyWrite(const CurlSocketListener &listener)
{
  handleMultiSocketAction( listener.nativeHandle(), CURL_CSELECT_OUT );
}

void HttpRequestDispatcherPrivate::onSocketError(const CurlSocketListener &listener)
{
  handleMultiSocketAction( listener.nativeHandle(), CURL_CSELECT_ERR );
}

void HttpRequestDispatcherPrivate::handleMultiSocketAction(curl_socket_t nativeSocket, int evBitmask)
{
  int running = 0;
  CURLMcode rc = curl_multi_socket_action( _multi, nativeSocket, evBitmask, &running );
  if (rc != 0) {
    //we can not recover from a error like that, cancel all and stop
    HttpRequestError err(  HttpRequestError::CurlMError, rc );
    cancelAll( err );
    //emit error
    _lastError = err;
    _sigError.emit( *z_func() );
    return;
  }

  int msgs_left = 0;
  CURLMsg *msg = nullptr;
  while( (msg = curl_multi_info_read( _multi, &msgs_left )) ) {
    if(msg->msg == CURLMSG_DONE) {
      CURL *easy = msg->easy_handle;
      CURLcode res = msg->data.result;

      void *privatePtr = nullptr;
      if ( curl_easy_getinfo( easy, CURLINFO_PRIVATE, &privatePtr ) != CURLE_OK )
        continue;

      if ( !privatePtr ) {
        //broken easy handle not associated, should never happen but clean it up
        std::cerr << "BROKEN EASY HANDLE, where did it come from " << std::endl;
        curl_multi_remove_handle( _multi, easy );
        curl_easy_cleanup( easy );
        continue;
      }

      HttpDownloadRequestPrivate *request = reinterpret_cast<HttpDownloadRequestPrivate *>( privatePtr );

      //trigger notification about file downloaded
      HttpRequestError::Type t = res == CURLE_OK ? HttpRequestError::NoError : HttpRequestError::CurlError;
      setFinished( *request->z_func(), HttpRequestError( t, res ) );

      //attention request could be deleted from here on
    }
  }

  if ( running == 0 && _pendingDownloads.size() == 0 ) {
    //once we finished all requests, cancel the timer wait so the event loop can exit
    _timer->cancel();
  } else {
    dequeuePending();
  }
}

void HttpRequestDispatcherPrivate::cancelAll( HttpRequestError result )
{
  while ( _runningDownloads.size() ) {
    std::shared_ptr<HttpDownloadRequest> &req = _runningDownloads.back();
    setFinished(*req, result );
  }
  while ( _pendingDownloads.size() ) {
    std::shared_ptr<HttpDownloadRequest> &req = _pendingDownloads.back();
    setFinished(*req, result );
  }
  _timer->cancel();
}

void HttpRequestDispatcherPrivate::setFinished( HttpDownloadRequest &req, HttpRequestError result )
{
  auto delReq = []( auto &list, HttpDownloadRequest &req ) {
    auto it = std::find_if( list.begin(), list.end(), [ &req ]( const std::shared_ptr<HttpDownloadRequest> &r ) {
      return req.d_func() == r->d_func();
    } );
    if ( it != list.end() ) {
      list.erase( it );
    }
  };

  //keep the request alive as long as we need it
  std::shared_ptr<HttpDownloadRequest> reqLocked = req.shared_this<HttpDownloadRequest>();
  delReq( _runningDownloads, req );
  delReq( _pendingDownloads, req );

  void *easyHandle = req.d_func()->_easyHandle;
  if ( easyHandle ) {
    curl_multi_remove_handle( _multi, easyHandle );

    //will reset to defaults but keep live connections, session ID and DNS caches
    if ( easyHandle )
      curl_easy_reset( easyHandle );

    curl_easy_cleanup( easyHandle );
  }

  req.d_func()->_easyHandle = nullptr;
  req.d_func()->_dispatcher = nullptr;

  //first set the result, the Request might have a checksum to check as well so a currently
  //successful request could fail later on
  req.d_func()->setResult( std::move(result) );
  _sigDownloadFinished.emit( *z_func(), req );
}

void HttpRequestDispatcherPrivate::dequeuePending()
{
  if ( !_isRunning )
    return;

  while ( _maxConnections > _runningDownloads.size() ) {
    if ( !_pendingDownloads.size() )
      break;

    std::shared_ptr<HttpDownloadRequest> req = std::move( _pendingDownloads.front() );
    _pendingDownloads.pop_front();

    if ( !req->d_func()->initialize( curl_easy_init() ) ) {
      req->d_func()->setResult( HttpRequestError( HttpRequestError::InternalError, -1, "Failed to initialize easy handle" ) );
      continue;
    }
    curl_multi_add_handle( _multi, req->d_func()->_easyHandle );

    req->d_func()->aboutToStart();
    _sigDownloadStarted.emit( *z_func(), *req );

    _runningDownloads.push_back( std::move(req) );
  }

  //check for empty queues here?
}

HttpRequestDispatcher::HttpRequestDispatcher( std::string targetDir )
  : Base( * new HttpRequestDispatcherPrivate ( std::move(targetDir) ) )
{

}

void HttpRequestDispatcher::enqueue(const std::shared_ptr<HttpDownloadRequest> &req )
{
  if ( !req )
    return;

  req->d_func()->_dispatcher = this;
  d_func()->_pendingDownloads.push_back( req );

  //dequeue if running and we have capacity
  d_func()->dequeuePending();
}

void HttpRequestDispatcher::cancel( HttpDownloadRequest &req, const std::string &reason )
{
  Z_D();

  if ( req.d_func()->_dispatcher != this ) {
    //TODO throw exception
    return;
  }

  d->setFinished( req, HttpRequestError( HttpRequestError::Cancelled, -1, reason.size() ? reason : "Request explicitely cancelled" ) );
}

void HttpRequestDispatcher::run( boost::asio::io_context *ctx )
{
  Z_D();

  // only once
  if ( d->_isRunning )
    return;

  d->_ioContext = ctx;
  d->_isRunning = true;

  if ( !d->_timer )
    d->_timer = std::make_unique<asio::deadline_timer>( *ctx );

  d->_multi = curl_multi_init();
  curl_multi_setopt( d->_multi, CURLMOPT_TIMERFUNCTION, HttpRequestDispatcherPrivate::multi_timer_cb );
  curl_multi_setopt( d->_multi, CURLMOPT_TIMERDATA, reinterpret_cast<void *>( d ) );
  curl_multi_setopt( d->_multi, CURLMOPT_SOCKETFUNCTION, HttpRequestDispatcherPrivate::socket_callback );
  curl_multi_setopt( d->_multi, CURLMOPT_SOCKETDATA, reinterpret_cast<void *>( d ) );

  d->dequeuePending();
}

const zyppng::HttpRequestError &HttpRequestDispatcher::lastError() const
{
  return d_func()->_lastError;
}

SignalProxy<void (const HttpRequestDispatcher &, const HttpDownloadRequest &)> HttpRequestDispatcher::sigDownloadStarted()
{
  return d_func()->_sigDownloadStarted;
}

SignalProxy<void (const HttpRequestDispatcher &, const HttpDownloadRequest &)> HttpRequestDispatcher::sigDownloadFinished()
{
  return d_func()->_sigDownloadFinished;
}

SignalProxy<void (const HttpRequestDispatcher &)> HttpRequestDispatcher::sigQueueFinished()
{
  return d_func()->_sigQueueFinished;
}

SignalProxy<void (const HttpRequestDispatcher &)> HttpRequestDispatcher::sigError()
{
  return d_func()->_sigError;
}

}
