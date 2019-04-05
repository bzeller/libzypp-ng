#ifndef ZYPP_NG_MEDIA_CURL_PRIVATE_REQUEST_P_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_PRIVATE_REQUEST_P_H_INCLUDED

#include <core/private/base_p.h>
#include <media/curl/request.h>
#include <curl/curl.h>
#include <array>

#include <zypp/Digest.h>

namespace zyppng {

  class HttpDownloadRequestPrivate : public BasePrivate
  {
  public:
    ZYPP_DECLARE_PUBLIC(HttpDownloadRequest)

    HttpDownloadRequestPrivate( Url &&url, std::string &&targetDir, off_t &&start, off_t &&len );
    virtual ~HttpDownloadRequestPrivate();

    void initialize (void *easyHandle );
    void aboutToStart ();
    void setResult ( HttpRequestError &&err );
    void reset ();

    Url   _url;        //file URL
    std::string _targetDir; //target directory

    off_t _start = 0;  //start offset of block to request
    off_t _len   = 0;  //len of block to request ( 0 if full length )

    std::shared_ptr<zypp::Digest> _digest; //digest to be used to calculate checksum
    std::string _expectedChecksum; //checksum to be expected after download is finished

    HttpDownloadRequest::State _state = HttpDownloadRequest::Pending;
    HttpRequestError _result;
    std::array<char, CURL_ERROR_SIZE+1> _errorBuf; //provide a buffer for a nicely formatted error

    FILE *_outFile = nullptr;
    void *_easyHandle = nullptr; // the easy handle that controlling this request
    HttpRequestDispatcher *_dispatcher = nullptr; // the parent downloader owning this request

    //signals
    signal<void ( const HttpDownloadRequest &req )> _sigStarted;
    signal<void ( const HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow )> _sigProgress;
    signal<void ( const HttpDownloadRequest &req, const HttpRequestError &err )> _sigFinished;

    static int curlProgressCallback ( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );
    static size_t writeCallback ( char *ptr, size_t size, size_t nmemb, void *userdata );
  };
}

#endif
