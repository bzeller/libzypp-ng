#include <media/curl/private/request_p.h>
#include <media/curl/curl.h>
#include <iostream>
#include <zypp/Pathname.h>
#include <stdio.h>
#include <fcntl.h>
#include <strstream>

namespace zyppng {

  std::vector<char> peek_data_fd( FILE *fd, off_t offset, size_t count )
  {
    if ( !fd )
      return {};

    fflush( fd );

    std::vector<char> data( count + 1 , '\0' );

    ssize_t l = -1;
    while ((l = pread( fileno( fd ), data.data(), count, offset ) ) == -1 && errno == EINTR)
      ;
    if (l == -1)
      return {};

    return data;
  }

  HttpDownloadRequestPrivate::HttpDownloadRequestPrivate(Url &&url, zypp::Pathname &&targetFile, off_t &&start, off_t &&len, HttpDownloadRequest::FileMode fMode )
    : _url ( std::move(url) )
    , _targetFile ( std::move( targetFile) )
    , _start ( std::move(start) )
    , _len ( std::move(len) )
    , _fMode ( std::move(fMode) )
    , _headers( std::unique_ptr< curl_slist, decltype (&curl_slist_free_all) >( nullptr, &curl_slist_free_all ) )
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

    if ( _headers )
      curl_easy_setopt( _easyHandle, CURLOPT_HTTPHEADER, _headers.get() );

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
        if ( _digest->digestVector() != _expectedChecksum ) {
          _state = HttpDownloadRequest::Error;

          std::string e;
          for (int j = 0; j < _expectedChecksum.size(); j++)
	    e += zypp::str::form("%02hhx", _expectedChecksum[j]);

	  std::string d;
          for (int j = 0; j < _digest->digestVector().size(); j++)
	    d += zypp::str::form("%02hhx", _digest->digestVector()[j]);

          _result = HttpRequestError( HttpRequestError::InvalidChecksum, -1, (zypp::str::Format("Invalid checksum %1% (%2%), expected checksum %3%") % _digest->digest() % e % d ) );
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
    _downloaded = 0;
    _reportedSize = 0;
    _errorBuf.fill( 0 );
  }

  int HttpDownloadRequestPrivate::curlProgressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
  {
    if ( !clientp )
      return 0;
    HttpDownloadRequestPrivate *that = reinterpret_cast<HttpDownloadRequestPrivate *>( clientp );
    that->_downloaded   = dlnow;
    that->_reportedSize = dltotal;
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
      std::string openMode = "w+b";
      if ( that->_fMode == HttpDownloadRequest::WriteShared )
        openMode = "r+b";

      that->_outFile = fopen( that->_targetFile.asString().c_str() , openMode.c_str() );

      //if the file does not exist create a new one
      if ( !that->_outFile && that->_fMode == HttpDownloadRequest::WriteShared ) {
        that->_outFile = fopen( that->_targetFile.asString().c_str() , "w+b" );
      }

      if ( !that->_outFile ) {
        strncpy( that->_errorBuf.data(), "Unable to open target file.", CURL_ERROR_SIZE);
        return 0;
      }

      if ( that->_start > 0 )
        if ( fseek( that->_outFile, that->_start, SEEK_SET ) != 0 ) {
          strncpy( that->_errorBuf.data(), "Unable to set output file pointer.", CURL_ERROR_SIZE);
          return 0;
        }
    }

     size_t written = fwrite( ptr, size, nmemb, that->_outFile );
     if ( that->_digest ) {
       that->_digest->update( ptr, written );
     }

     return written;
  }

  HttpDownloadRequest::HttpDownloadRequest(zyppng::Url url, zypp::filesystem::Pathname targetFile, off_t start, off_t len, zyppng::HttpDownloadRequest::FileMode fMode)
    : Base ( *new HttpDownloadRequestPrivate( std::move(url), std::move(targetFile), std::move(start), std::move(len), std::move(fMode) ) )
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

  void HttpDownloadRequest::setPriority(HttpDownloadRequest::Priority prio)
  {
    d_func()->_priority = prio;
  }

  HttpDownloadRequest::Priority HttpDownloadRequest::priority() const
  {
    return d_func()->_priority;
  }

  void *HttpDownloadRequest::nativeHandle() const
  {
    return d_func()->_easyHandle;
  }

  std::vector<char> HttpDownloadRequest::peekData( off_t offset, size_t count ) const
  {
    Z_D();
    return peek_data_fd( d->_outFile, offset, count );
  }

  Url HttpDownloadRequest::url() const
  {
    return d_func()->_url;
  }

  void HttpDownloadRequest::setUrl(const Url &url)
  {
    Z_D();
    if ( d->_state == HttpDownloadRequest::Running )
      return;

    d->_url = url;
  }

  const zypp::filesystem::Pathname &HttpDownloadRequest::targetFilePath() const
  {
    return d_func()->_targetFile;
  }

  std::string HttpDownloadRequest::contentType() const
  {
    char *ptr = NULL;
    if ( curl_easy_getinfo( d_func()->_easyHandle, CURLINFO_CONTENT_TYPE, &ptr ) == CURLE_OK && ptr )
      return std::string(ptr);
    return std::string();
  }

  off_t HttpDownloadRequest::downloadOffset() const
  {
    return d_func()->_start;
  }

  off_t HttpDownloadRequest::reportedByteCount() const
  {
    return d_func()->_reportedSize;
  }

  off_t HttpDownloadRequest::expectedByteCount() const
  {
    return d_func()->_len;
  }

  off_t HttpDownloadRequest::downloadedByteCount() const
  {
    return d_func()->_downloaded;
  }

  void HttpDownloadRequest::setDigest( std::shared_ptr<zypp::Digest> dig )
  {
    d_func()->_digest = dig;
  }

  void HttpDownloadRequest::setExpectedChecksum(std::vector<unsigned char> checksum )
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

  bool HttpDownloadRequest::addRequestHeader( std::string &&header )
  {
    Z_D();

    curl_slist *res = curl_slist_append( d->_headers ? d->_headers.get() : nullptr, header.c_str() );
    if ( !res )
      return false;

    if ( !d->_headers )
      d->_headers = std::unique_ptr< curl_slist, decltype (&curl_slist_free_all) >( res, &curl_slist_free_all );
  }

  SignalProxy<void (HttpDownloadRequest &req)> HttpDownloadRequest::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)> HttpDownloadRequest::sigProgress()
  {
    return d_func()->_sigProgress;
  }

  SignalProxy<void (zyppng::HttpDownloadRequest &req, const zyppng::HttpRequestError &err)> HttpDownloadRequest::sigFinished()
  {
    return d_func()->_sigFinished;
  }


}
