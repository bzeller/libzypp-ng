#include <media/curl/private/request_p.h>
#include <media/curl/curl.h>
#include <iostream>
#include <zypp/Pathname.h>

namespace zyppng {

  HttpDownloadRequestPrivate::HttpDownloadRequestPrivate(Url &&url, std::string &&targetDir, off_t &&start, off_t &&len)
    : _url ( std::move(url) )
    , _targetDir ( std::move( targetDir) )
    , _start ( std::move(start) )
    , _len ( std::move(len) )
  {
  }

  HttpDownloadRequestPrivate::~HttpDownloadRequestPrivate()
  {
  }

  bool HttpDownloadRequestPrivate::initialize( void *easyHandle )
  {
    reset();

    _easyHandle = easyHandle;
    curl_easy_setopt( _easyHandle, CURLOPT_PRIVATE, this );
    curl_easy_setopt( _easyHandle, CURLOPT_XFERINFOFUNCTION, HttpDownloadRequestPrivate::curlProgressCallback );
    curl_easy_setopt( _easyHandle, CURLOPT_XFERINFODATA, this  );
    curl_easy_setopt( _easyHandle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt( _easyHandle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt( _easyHandle, CURLOPT_NOSIGNAL, 1L);
    // follow any Location: header that the server sends as part of
    // an HTTP header (#113275)
    curl_easy_setopt( _easyHandle, CURLOPT_FOLLOWLOCATION, 1L);
    // 3 redirects seem to be too few in some cases (bnc #465532)
    curl_easy_setopt( _easyHandle, CURLOPT_MAXREDIRS, 6L );

    /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
    //curl_easy_setopt( _easyHandle, CURLOPT_VERBOSE, 1L );

    std::string urlBuffer( _url.asString() );
    curl_easy_setopt( _easyHandle, CURLOPT_URL, urlBuffer.c_str() );

    curl_easy_setopt( _easyHandle, CURLOPT_WRITEFUNCTION, HttpDownloadRequestPrivate::writeCallback );
    curl_easy_setopt( _easyHandle, CURLOPT_WRITEDATA, this );

    _errorBuf.fill( '\0' );
    curl_easy_setopt( _easyHandle, CURLOPT_ERRORBUFFER, this->_errorBuf.data() );


    std::string rangeDesc;
    if ( _start >= 0) {
      _expectRangeStatus = true;
      rangeDesc = zypp::str::form("%llu-", static_cast<unsigned long long>( _start ));
      if( _len > 0 ) {
        rangeDesc.append( zypp::str::form( "%llu", static_cast<unsigned long long>(_start + _len - 1) ) );
      }
      if ( curl_easy_setopt( _easyHandle, CURLOPT_RANGE, rangeDesc.c_str() ), CURLE_OK ) {
        strncpy( _errorBuf.data(), "curl_easy_setopt range failed", CURL_ERROR_SIZE);
        return false;
      }
    } else {
      _expectRangeStatus = false;
    }
    return true;
  }

  void HttpDownloadRequestPrivate::aboutToStart()
  {
    _state = HttpDownloadRequest::Running;
    _sigStarted.emit( *z_func() );
  }

  void HttpDownloadRequestPrivate::setResult( HttpRequestError &&err )
  {
    if ( _outFile )
      fclose( _outFile );
    _outFile = nullptr;

    _result = std::move(err);

    if ( err.type() == HttpRequestError::NoError ) {
      //we have a successful download, lets see if the checksum is fine IF we have one
      _state = HttpDownloadRequest::Finished;
      if ( _expectedChecksum.size() && _digest ) {
        if ( _digest->digest() != _expectedChecksum ) {
          _state = HttpDownloadRequest::Error;
          _result = HttpRequestError( HttpRequestError::InvalidChecksum, -1, (zypp::str::Format("Invalid checksum %1%, expected checksum %2%") % _digest->digest() % _expectedChecksum ) );
        }
      }
    } else
      _state = HttpDownloadRequest::Error;

    _sigFinished.emit( *z_func(), _result );
  }

  void HttpDownloadRequestPrivate::reset()
  {
    if ( _outFile )
      fclose( _outFile );

    if ( _digest )
      _digest->reset();

    _outFile = nullptr;
    _easyHandle = nullptr;
    _result = HttpRequestError();
    _state = HttpDownloadRequest::Pending;
    _errorBuf.fill( 0 );
  }

  int HttpDownloadRequestPrivate::curlProgressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
  {
    if ( !clientp )
      return 0;
    HttpDownloadRequestPrivate *that = reinterpret_cast<HttpDownloadRequestPrivate *>( clientp );
    that->_sigProgress.emit( *that->z_func(), dltotal, dlnow, ultotal, ulnow );
    return 0;
  }

  size_t HttpDownloadRequestPrivate::writeCallback( char *ptr, size_t size, size_t nmemb, void *userdata )
  {
    if ( !userdata )
      return 0;

    //it is valid to call this function with no data to write, just return OK
    if ( size * nmemb == 0)
      return 0;

    HttpDownloadRequestPrivate *that = reinterpret_cast<HttpDownloadRequestPrivate *>( userdata );

    //If we expect a file range we better double check that we got the status code for it
    if ( that->_expectRangeStatus ) {
      char *effurl;
      (void)curl_easy_getinfo( that->_easyHandle, CURLINFO_EFFECTIVE_URL, &effurl);
      if (effurl && !strncasecmp(effurl, "http", 4))
      {
        long statuscode = 0;
        (void)curl_easy_getinfo( that->_easyHandle, CURLINFO_RESPONSE_CODE, &statuscode);
        if (statuscode != 206)
          return 0;
      }
    }

    if ( !that->_outFile ) {
      zypp::Pathname p( that->_url.getPathName( ) );
      that->_outFile = fopen( ( that->_targetDir + "/" + p.basename() ).c_str(), "w");
    }

     size_t written = fwrite( ptr, size, nmemb, that->_outFile );

     if ( that->_digest ) {
       that->_digest->update( ptr, written );
     }

     return written;
  }

  HttpDownloadRequest::HttpDownloadRequest(zyppng::Url url, std::string dir, off_t start, off_t len)
    : Base ( *new HttpDownloadRequestPrivate( std::move(url), std::move(dir), std::move(start), std::move(len) ) )
  {
  }

  HttpDownloadRequest::~HttpDownloadRequest()
  {
    Z_D();

    if ( d->_dispatcher )
      d->_dispatcher->cancel( *this, "Request destroyed while still running" );

    if ( d->_outFile )
      fclose( d->_outFile );
  }

  Url HttpDownloadRequest::url() const
  {
    return d_func()->_url;
  }

  void HttpDownloadRequest::setDigest( std::shared_ptr<zypp::Digest> dig )
  {
    d_func()->_digest = dig;
  }

  void HttpDownloadRequest::setExpectedChecksum( std::string checksum )
  {
    d_func()->_expectedChecksum = std::move(checksum);
  }

  std::shared_ptr<zypp::Digest> HttpDownloadRequest::digest( ) const
  {
    return d_func()->_digest;
  }

  HttpDownloadRequest::State HttpDownloadRequest::state() const
  {
    return d_func()->_state;
  }

  const HttpRequestError &HttpDownloadRequest::error() const
  {
    return d_func()->_result;
  }

  std::string HttpDownloadRequest::extendedErrorString() const
  {
    Z_D();
    if ( !hasError() )
      return std::string();
    if ( d->_result.type() == HttpRequestError::CurlError && d->_errorBuf[0] )
      return std::string( d->_errorBuf.data() );
    return std::string();
  }

  bool HttpDownloadRequest::hasError() const
  {
    return error().isError();
  }

  SignalProxy<void (const HttpDownloadRequest &req)> HttpDownloadRequest::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (const HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)> HttpDownloadRequest::sigProgress()
  {
    return d_func()->_sigProgress;
  }

  SignalProxy<void (const zyppng::HttpDownloadRequest &req, const zyppng::HttpRequestError &err)> HttpDownloadRequest::sigFinished()
  {
    return d_func()->_sigFinished;
  }

}
