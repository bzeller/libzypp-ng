#ifndef ZYPP_NG_MEDIA_CURL_DOWNLOADER_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_DOWNLOADER_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/base.h>
#include <core/url.h>
#include <core/signals.h>

#include <zypp/ByteCount.h>

namespace boost {
  namespace asio {
    class io_context;
  }
}

namespace zypp {
  namespace media {
    class TransferSettings;
  }
}

namespace zyppng {

  class DownloaderPrivate;
  class Download;

  using TransferSettings = zypp::media::TransferSettings;

  /**
   * @brief The Downloader class
   *
   * Provides a high level interface to the \sa HttpRequestDispatcher,
   * implementing Metalink on top. If in doubt which one to use, always
   * use this one.
   */
  class LIBZYPP_NG_EXPORT Downloader : public Base
  {
    ZYPP_DECLARE_PRIVATE( Downloader )
  public:
    Downloader( boost::asio::io_context &ctx );

    std::shared_ptr<Download> downloadFile ( std::vector<Url> mirrors, zypp::filesystem::Pathname targetPath, zypp::ByteCount expectedFileSize = zypp::ByteCount() );

    void disableMetalinkHandling ();

    SignalProxy<void ( Downloader &parent, Download& download )> sigStarted  ( );
    SignalProxy<void ( Downloader &parent, Download& download )> sigFinished ( );
    SignalProxy<void ( Downloader &parent )> queueEmpty ( );
  };

  class DownloadPrivate;
  class LIBZYPP_NG_EXPORT Download : public Base
  {
    ZYPP_DECLARE_PRIVATE( Download )

  public:

    enum State {
      Initializing,
      Running,
      RunningMulti,
      Success,
      Failed
    };


    Download ( DownloadPrivate &prv );

    zypp::Pathname targetPath () const;

    State state () const;

    std::string errorString () const;

    /**
     * @brief sigStarted
     * Signals that the dispatcher dequeued the request and actually starts downloading data
     */
    SignalProxy<void ( Download &req )> sigStarted  ();

    /**
     * @brief sigStateChanged
     * Signals that the state of the \a Download has changed
     */
    SignalProxy<void ( Download &req, State state )> sigStateChanged  ();

    /**
     * @brief sigAlive
     * Signals that the download is alive but still in initial stage ( trying to figure out if metalink / zsync )
     */
    SignalProxy<void ( Download &req )> sigAlive ();

    /**
     * @brief sigProgress
     * Signals if there was data read from the download
     */
    SignalProxy<void ( Download &req, off_t dltotal, off_t dlnow )> sigProgress ();

    /**
     * @brief sigFinished
     * Signals that the download finished.
     *
     * \note After this signal was emitted the Curl handle is reset,
     *       so all queries to it need to be done in a Slot connected to this signal
     */
    SignalProxy<void ( Download &req )> sigFinished ( );


  };


}

#endif
