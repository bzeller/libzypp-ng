#ifndef ZYPP_NG_MEDIA_CURL_PRIVATE_CURL_P_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_PRIVATE_CURL_P_H_INCLUDED

#include <media/curl/curl.h>
#include <core/private/base_p.h>
#include <curl/curl.h>
#include <deque>
#include <set>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

namespace boost {
  namespace asio {
    namespace posix {
      class stream_descriptor;
    }
  }
}

namespace zyppng {

  class CurlSocketListener : std::enable_shared_from_this<CurlSocketListener>
  {
  public:

    enum Mode {
      Disabled  = 0,
      WaitRead  = 1,
      WaitWrite = 2,
      WaitErr   = 4
    };

    /*
     * Use a Handler functor instead of a normal handler to work around the
     * problem that even if "cancel" is called on a posix::stream_descriptor a event can already be
     * in the mainloop. This handler is passed a shared_ptr to itself bound to the event callback function.
     * This way the Handler is always valid. In case the SocketListener is disabled it can set the "enabled" bit
     * to false and remove its reference. The handler will check for the enabled bit before doing anything else and
     * return. Once the bound reference is cleared up the Handler is finally destroyed.
     */
    struct Handler {
      bool enabled = false;
      CurlSocketListener *parent = nullptr;
      Mode mode = Disabled;
      static void staticCallback ( std::shared_ptr<Handler> that, const boost::system::error_code &ec );
    private:
      void handler ( std::shared_ptr<CurlSocketListener::Handler> that, const boost::system::error_code &ec  );
    };

    CurlSocketListener( boost::asio::io_context &ctx, curl_socket_t s, Mode waitMode = Disabled );
    ~CurlSocketListener();

    void setMode ( int mode );
    curl_socket_t nativeHandle () const;

    SignalProxy<void (const CurlSocketListener &)> sigReadyRead();
    SignalProxy<void (const CurlSocketListener &)> sigReadyWrite();
    SignalProxy<void (const CurlSocketListener &)> sigError();

  private:
    signal<void (const CurlSocketListener &)> _readyRead;
    signal<void (const CurlSocketListener &)> _readyWrite;
    signal<void (const CurlSocketListener &)> _error;

    std::shared_ptr<Handler> _readHandler;
    std::shared_ptr<Handler> _writeHandler;
    std::shared_ptr<Handler> _errHandler;
    std::unique_ptr<boost::asio::posix::stream_descriptor> _strDesc;
    curl_socket_t _socket;
  };

  class HttpRequestDispatcherPrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(HttpRequestDispatcher)
  public:
    HttpRequestDispatcherPrivate (  boost::asio::io_context &ctx  );
    virtual ~HttpRequestDispatcherPrivate();

    size_t _maxConnections = 10;

    std::deque< std::shared_ptr<HttpDownloadRequest> > _pendingDownloads;
    std::vector< std::shared_ptr<HttpDownloadRequest> > _runningDownloads;

    boost::asio::io_context *_ioContext = nullptr;
    std::unique_ptr<boost::asio::deadline_timer> _timer;
    std::map< curl_socket_t, std::shared_ptr<CurlSocketListener> > _socketHandler;

    bool  _isRunning = false;
    CURLM *_multi = nullptr;

    HttpRequestError _lastError;

    //signals
    signal<void ( const HttpRequestDispatcher &, const HttpDownloadRequest & )> _sigDownloadStarted;
    signal<void ( const HttpRequestDispatcher &, const HttpDownloadRequest & )> _sigDownloadFinished;
    signal<void ( const HttpRequestDispatcher & )> _sigQueueFinished;
    signal<void ( const HttpRequestDispatcher & )> _sigError;

  private:
    static int multi_timer_cb ( CURLM *multi, long timeout_ms, void *g );
    static int socket_callback(CURL *easy, curl_socket_t s, int what, void *userp, CurlSocketListener *socketp );

    void cancelAll ( HttpRequestError result );
    void setFinished( HttpDownloadRequest &req , HttpRequestError result );

    void onSocketReadyRead  ( const CurlSocketListener &listener );
    void onSocketReadyWrite ( const CurlSocketListener &listener );
    void onSocketError ( const CurlSocketListener &listener );

    void handleMultiSocketAction ( curl_socket_t nativeSocket, int evBitmask );
    void dequeuePending ();
  };
}

#endif
