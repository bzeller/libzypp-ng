#include <media/curl/private/downloader_p.h>
#include <media/curl/private/request_p.h>
#include <media/curl/curl.h>
#include <media/curl/request.h>
#include <zypp/Pathname.h>
#include <zypp/media/TransferSettings.h>
#include <zypp/media/MetaLinkParser.h>
#include <zypp/ByteCount.h>
#include <zypp/base/String.h>

#include <queue>
#include <fcntl.h>
#include <iostream>
#include <fstream>

namespace  {
  bool looks_like_metalink_data( const std::vector<char> &data )
  {
    if ( data.empty() )
      return false;

    const char *p = data.data();
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      p++;

    if (!strncasecmp(p, "<?xml", 5))
    {
      while (*p && *p != '>')
        p++;
      if (*p == '>')
        p++;
      while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    }
    bool ret = !strncasecmp( p, "<metalink", 9 ) ? true : false;
    return ret;
  }

  bool looks_like_metalink_file( const zypp::Pathname &file )
  {
    std::unique_ptr<FILE, decltype(&fclose)> fd( fopen( file.c_str(), "r" ), &fclose );
    if ( !fd )
      return false;
    return looks_like_metalink_data( zyppng::peek_data_fd( fd.get(), 0, 256 ) );
  }
}

namespace zyppng {

  DownloadPrivate::DownloadPrivate( Downloader &parent, std::shared_ptr<HttpRequestDispatcher> requestDispatcher, std::vector<Url> &&mirrors, zypp::filesystem::Pathname &&targetPath, zypp::ByteCount &&expectedFileSize )
    : _parent( &parent )
    , _requestDispatcher ( requestDispatcher )
    , _mirrors( std::move(mirrors) )
    , _targetPath( std::move(targetPath) )
    , _expectedFileSize( std::move(expectedFileSize) )
  {
    start();
  }

  void DownloadPrivate::start()
  {
    //use the first URL for now
    std::shared_ptr<HttpDownloadRequest> initialRequest = std::make_shared<HttpDownloadRequest>( _mirrors.front(), _targetPath );
    initialRequest->addRequestHeader("Accept: */*, application/metalink+xml, application/metalink4+xml");
    _state = Download::Initializing;
    _isMultiDownload = false;

    addNewRequest( initialRequest );
  }

  void DownloadPrivate::onRequestStarted( HttpDownloadRequest & )
  {
    if ( _state == Download::Initializing )
      _sigStarted.emit( *z_func() );
  }

  void DownloadPrivate::onRequestProgress( HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t , off_t  )
  {
    //we are not sure yet if we are downloading a Metalink, for now just send alive messages
    if ( _state == Download::Initializing ) {
      std::cout << "Checking for multi download " << std::endl;
      if ( !_isMultiDownload ) {
        std::string cType = req.contentType();
        if ( cType.find("application/metalink+xml") == 0 || cType.find("application/metalink4+xml") == 0 )
          _isMultiDownload = true;
      }

      std::cout << "Checking for multi download 2" << std::endl;
      if ( !_isMultiDownload && dlnow < 256 ) {
        // can't tell yet, ...
        return _sigAlive.emit( *z_func() );
      }

      std::cout << "Checking for multi download 3" << std::endl;
      if ( !_isMultiDownload )
      {
        _isMultiDownload = looks_like_metalink_data( req.peekData( 0, 256 ) );
        std::cerr << "looks_like_metalink_fd: " << _isMultiDownload << std::endl;
      }

      if ( _isMultiDownload )
      {
        // this is a metalink file change the expected filesize
        if ( zypp::ByteCount( 2, zypp::ByteCount::MB) < static_cast<zypp::ByteCount::SizeType>( dlnow ) ) {
          _requestDispatcher->cancel( req, HttpRequestError( HttpRequestError::ExceededMaxLen, -1 ) );
          return;
        }
        return _sigAlive.emit( *z_func() );
      }

      //if we reach here we have a normal file ( or a multi download with more than 256 byte of comment in the beginning )
      _state = Download::Running;
      _sigStateChanged.emit( *z_func(), _state );

    }

    if ( _state == Download::Running ) {
      if ( _expectedFileSize > 0 && _expectedFileSize < dlnow ) {
        _requestDispatcher->cancel( req, HttpRequestError( HttpRequestError::ExceededMaxLen, -1 ) );
        return;
      }
      return _sigProgress.emit( *z_func(), dltotal, dlnow );

    } else if ( _state == Download::RunningMulti ) {
      _downloadedMultiByteCount += dlnow;
      off_t dlTotal = _blockList.getFilesize();
      _sigProgress.emit( *z_func(), dlTotal, _downloadedMultiByteCount );

    }
  }

  void DownloadPrivate::onRequestFinished( HttpDownloadRequest &req, const zyppng::HttpRequestError &err )
  {

    std::cout << "Request finished " << std::endl;
    auto reqLocked = req.shared_this<HttpDownloadRequest>();
    auto it = std::find( _runningRequests.begin(), _runningRequests.end(), reqLocked );
    if ( it == _runningRequests.end() )
      return;

    std::cout << " KNOWN Request finished " << std::endl;

    //remove from running
    _runningRequests.erase( it );

    if ( err.isError() ) {
      std::cout << "Request has error " << err.toString() << std::endl;
      if ( _state == Download::RunningMulti ) {
        //we need to adjust the downloaded bytecount so it does not contain the bytes
        //that need to be downloaded again
        _downloadedMultiByteCount -= req.downloadedByteCount();
      }

      if ( _mirrors.size() > 1 ) {
        auto currMirr = std::find( _mirrors.begin(), _mirrors.end(), req.url() );
        //use the next mirror
        currMirr++;
        if ( false /*currMirr != _mirrors.end()*/ ) {
          std::cout << "Using next mirror " << *currMirr << std::endl;
          reqLocked->setUrl( *currMirr );
          //make sure this request will run asap
          reqLocked->setPriority( HttpDownloadRequest::High );

          //this is not a new request, only add to queues but do not connect signals again
          _runningRequests.push_back( reqLocked );
          _requestDispatcher->enqueue( reqLocked );
          return;
        }
      }

      //we do not have more mirrors left we can try, abort
      while( _runningRequests.size() ) {
        auto req = _runningRequests.back();
        //remove the request from the running queue BEFORE calling cancel, otherwise we end in a busyloop
        _runningRequests.pop_back();
        _requestDispatcher->cancel( *req, err );
      }

      //TODO if this is a multi download fall back to a normal one

      _requestError = err;
      setFailed( "Download failed ");
      return;
    }

    if ( _state == Download::Initializing || _state == Download::Running ) {
      if ( !_isMultiDownload )
        _isMultiDownload = looks_like_metalink_file( req.targetFilePath() );
      if ( !_isMultiDownload ) {
        _state = Download::Success;
        _sigStateChanged.emit( *z_func(), _state );
        _sigFinished.emit( *z_func() );
        return;
      }

      //we have a metalink download, lets parse it and see what we got
      try {
        zypp::media::MetaLinkParser parser;
        parser.parse( req.targetFilePath() );
        _mirrors   = parser.getUrls();
        _blockList = parser.getBlockList();
      } catch ( const zypp::Exception &ex ) {
        setFailed( zypp::str::Format("Failed to parse metalink information.(%1%)" ) % ex.asUserString() );
        return;
      }

      _state = Download::RunningMulti;
      _sigStateChanged.emit( *z_func(), _state );

      for ( size_t i = 0; i < _blockList.numBlocks(); i++ ) {
        zypp::media::MediaBlock blk = _blockList.getBlock( i );
        std::shared_ptr<HttpDownloadRequest> req = std::make_shared<HttpDownloadRequest>( _mirrors.front(), _targetPath, blk.off, blk.size, HttpDownloadRequest::WriteShared );
        req->setPriority( HttpDownloadRequest::High );

        std::shared_ptr<zypp::Digest> dig = std::make_shared<zypp::Digest>();
        _blockList.createDigest( *dig );
        req->setDigest( dig );
        req->setExpectedChecksum( _blockList.getChecksum(i) );
        addNewRequest( req );
      }
    }


    if ( _runningRequests.empty() ) {
      if ( _state == Download::RunningMulti && _blockList.haveFileChecksum() ) {
        //TODO move this into a external application so we do not need to block on it
        //need to check file digest
        zypp::Digest dig;
        _blockList.createFileDigest( dig );

        std::ifstream istrm( _targetPath.asString(), std::ios::binary);
        if ( !istrm.is_open() ) {
          setFailed( "Failed to verify file digest (Could not open target file)." );
          return;
        }
        if ( !dig.update( istrm ) ) {
          setFailed( "Failed to verify file digest (Could not read target file)." );
          return;
        }
        if ( !_blockList.verifyFileDigest( dig ) ) {
          setFailed( "Failed to verify file digest (Checksum did not match)." );
          return;
        }
      }
      //TODO implement digest check for non multi downloads
      _state = Download::Success;
      _sigStateChanged.emit( *z_func(), _state );
      _sigFinished.emit( *z_func() );
    }
  }

  void DownloadPrivate::addNewRequest( std::shared_ptr<HttpDownloadRequest> req )
  {
    req->sigStarted().connect ( sigc::mem_fun( *this, &DownloadPrivate::onRequestStarted) );
    req->sigProgress().connect( sigc::mem_fun( *this, &DownloadPrivate::onRequestProgress) );
    req->sigFinished().connect( sigc::mem_fun( *this, &DownloadPrivate::onRequestFinished) );
    _runningRequests.push_back( req );
    _requestDispatcher->enqueue( req );
  }

  void DownloadPrivate::setFailed(std::string &&reason)
  {
    _state = Download::Failed;
    _errorString = reason;
    _sigStateChanged.emit( *z_func(), _state );
    _sigFinished.emit( *z_func() );
  }

  Download::Download(DownloadPrivate &prv)
  : Base( prv )
  { }

  zypp::filesystem::Pathname Download::targetPath() const
  {
    return d_func()->_targetPath;
  }

  Download::State Download::state() const
  {
    return d_func()->_state;
  }

  std::string Download::errorString() const
  {
    Z_D();
    if (! d->_requestError.isError() ) {
      return d->_errorString;
    }
    return ( zypp::str::Format("%1%(%2%") % d->_errorString % d->_requestError.toString() );
  }

  SignalProxy<void (Download &req)> Download::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (Download &req, Download::State state)> Download::sigStateChanged()
  {
    return d_func()->_sigStateChanged;
  }

  SignalProxy<void (Download &req)> zyppng::Download::sigAlive()
  {
    return d_func()->_sigAlive;
  }

  SignalProxy<void (Download &req, off_t dltotal, off_t dlnow)> Download::sigProgress()
  {
    return d_func()->_sigProgress;
  }

  SignalProxy<void (Download &req)> Download::sigFinished()
  {
    return d_func()->_sigFinished;
  }

  Downloader::Downloader( boost::asio::io_context &ctx )
    : Base ( *new DownloaderPrivate( ctx ) )
  {

  }

  std::shared_ptr<Download> Downloader::downloadFile( std::vector<Url> mirrors, zypp::filesystem::Pathname targetPath, zypp::ByteCount expectedFileSize )
  {
    if ( mirrors.size() == 0 )
      return {};

    Z_D();
    std::shared_ptr<Download> dl = std::make_shared<Download>( *new DownloadPrivate( *this, d->_requestDispatcher, std::move(mirrors), std::move(targetPath), std::move(expectedFileSize) ) );

    d->_runningDownloads.push_back( dl );
    d->_requestDispatcher->run();

    return dl;
  }

  void Downloader::disableMetalinkHandling()
  {

  }

  SignalProxy<void (Downloader &parent, Download &download)> Downloader::sigStarted()
  {

  }

  SignalProxy<void (Downloader &parent, Download &download)> Downloader::sigFinished()
  {

  }

  SignalProxy<void (Downloader &parent)> Downloader::queueEmpty()
  {

  }

  DownloaderPrivate::DownloaderPrivate(boost::asio::io_context &ioCtx)
    : _ioContext( &ioCtx )
  {
    _requestDispatcher = std::make_shared<HttpRequestDispatcher>( ioCtx );
  }

}
