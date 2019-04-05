#ifndef ZYPP_NG_MEDIA_CURL_REQUEST_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_REQUEST_H_INCLUDED

#include <media/curl/httprequesterror.h>
#include <core/base.h>
#include <core/zyppglobal.h>
#include <core/signals.h>
#include <core/url.h>
#include <vector>

namespace zypp {
  class Digest;
}

namespace zyppng {

  class HttpRequestDispatcher;
  class HttpDownloadRequestPrivate;

  class LIBZYPP_NG_EXPORT HttpDownloadRequest : public Base
  {
  public:
    enum State {
      Pending,    // waiting to be dispatched
      Running,    // currently running
      Finished,   // finished successfully
      Error,      // Error, use error function to figure out the issue
    };

    HttpDownloadRequest( Url url, std::string dir, off_t start = -1, off_t len = 0 );
    virtual ~HttpDownloadRequest();

    Url url () const;
    void setDigest ( std::shared_ptr<zypp::Digest> dig );
    void setExpectedChecksum ( std::string checksum );
    std::shared_ptr<zypp::Digest> digest () const;

    State state () const;
    const HttpRequestError &error () const;
    std::string extendedErrorString() const;
    bool hasError () const;

    SignalProxy<void ( const HttpDownloadRequest &req )> sigStarted  ();
    SignalProxy<void ( const HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow )> sigProgress ();
    SignalProxy<void ( const HttpDownloadRequest &req, const HttpRequestError &err)> sigFinished ( );

  private:
    friend class HttpRequestDispatcher;
    friend class HttpRequestDispatcherPrivate;
    ZYPP_DECLARE_PRIVATE( HttpDownloadRequest )
  };

}

#endif
