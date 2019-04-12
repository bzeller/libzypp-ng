#ifndef ZYPP_NG_MEDIA_CURL_PRIVATE_DOWNLOADER_P_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_PRIVATE_DOWNLOADER_P_H_INCLUDED

#include <core/private/base_p.h>
#include <media/curl/downloader.h>
#include <zypp/media/MediaBlockList.h>
#include <media/curl/httprequesterror.h>

namespace zyppng {

  class HttpRequestDispatcher;
  class HttpDownloadRequest;

  class DownloadPrivate : public BasePrivate
  {
  public:
    ZYPP_DECLARE_PUBLIC(Download)
    DownloadPrivate ( Downloader &parent, std::shared_ptr<HttpRequestDispatcher> requestDispatcher, std::vector<Url> &&mirrors, zypp::filesystem::Pathname &&targetPath, zypp::ByteCount &&expectedFileSize );

    std::vector< std::shared_ptr<HttpDownloadRequest> > _runningRequests;

    std::shared_ptr<HttpRequestDispatcher> _requestDispatcher;
    std::vector<Url> _mirrors;
    zypp::filesystem::Pathname _targetPath;
    zypp::ByteCount _expectedFileSize;
    std::string _errorString;
    HttpRequestError _requestError;

    off_t _downloadedMultiByteCount = 0; //< the number of bytes that were already fetched in RunningMulti state
    zypp::media::MediaBlockList _blockList;

    Downloader *_parent = nullptr;
    Download::State _state = Download::Initializing;
    bool _isMultiDownload = false;

    signal<void ( Download &req )> _sigStarted;
    signal<void ( Download &req, Download::State state )> _sigStateChanged;
    signal<void ( Download &req )> _sigAlive;
    signal<void ( Download &req, off_t dltotal, off_t dlnow )> _sigProgress;
    signal<void ( Download &req )> _sigFinished;

  private:
    void start ();
    void onRequestStarted  ( HttpDownloadRequest & );
    void onRequestProgress ( HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t, off_t );
    void onRequestFinished ( HttpDownloadRequest &req , const HttpRequestError &err );
    void addNewRequest     ( std::shared_ptr<HttpDownloadRequest> req );
    void setFailed         ( std::string && reason );
  };

  class DownloaderPrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(Downloader)
  public:
    DownloaderPrivate( boost::asio::io_context &ioCtx );

    boost::asio::io_context *_ioContext = nullptr;
    std::vector< std::shared_ptr<Download> > _runningDownloads;
    std::shared_ptr<HttpRequestDispatcher> _requestDispatcher;

    signal<void ( Downloader &parent, Download& download )> _sigStarted;
    signal<void ( Downloader &parent, Download& download )> _sigFinished;
    signal<void ( Downloader &parent )> _queueEmpty;
  };

}

#endif
