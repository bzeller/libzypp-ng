#ifndef ZYPP_NG_MEDIA_CURL_CURL_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_CURL_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/base.h>
#include <core/url.h>
#include <core/signals.h>
#include <vector>

#include <media/curl/httprequesterror.h>

namespace boost {
  namespace asio {
    class io_context;
  }
}

namespace zyppng {

  class HttpRequestDispatcherPrivate;
  class HttpDownloadRequest;


  class LIBZYPP_NG_EXPORT HttpRequestDispatcher : public Base
  {
    ZYPP_DECLARE_PRIVATE(HttpRequestDispatcher)
    public:

      HttpRequestDispatcher (std::string targetDir );

      void enqueue ( const std::shared_ptr<HttpDownloadRequest> &req );
      void cancel  (HttpDownloadRequest &req , const std::string &reason = std::string() );
      void run ( boost::asio::io_context *ctx );

      const HttpRequestError &lastError() const;

      SignalProxy<void ( const HttpRequestDispatcher &, const HttpDownloadRequest & )> sigDownloadStarted  ();
      SignalProxy<void ( const HttpRequestDispatcher &, const HttpDownloadRequest & )> sigDownloadFinished ();
      SignalProxy<void ( const HttpRequestDispatcher & )> sigQueueFinished ();
      SignalProxy<void ( const HttpRequestDispatcher & )> sigError ();

  };

}


#endif
