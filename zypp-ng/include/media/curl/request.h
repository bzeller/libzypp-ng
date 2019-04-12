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

    enum Priority {
      Normal, // Requests with normal priority will be enqueued at the end
      High    // Request with high priority will be moved to the front of the queue
    };

    enum FileMode {
      WriteExclusive, //the request will create its own file, overwriting anything that already exists
      WriteShared   //the request will create open the file in shared mode and only write between \a start and \a len
    };

    HttpDownloadRequest( Url url, zypp::Pathname targetFile, off_t start = -1, off_t len = 0, FileMode fMode = WriteExclusive );
    virtual ~HttpDownloadRequest();

    void setPriority ( Priority prio );
    Priority priority ( ) const;

    /**
     * @brief nativeHandle
     * Returns a pointer to the native CURL easy handle
     *
     * \note consider adding the functionality here instead of using the Handle directly.
     *       In case we ever decide to switch out the CURL backend your code will break
     *
     * \note this will only return a valid pointer after the \a started and before
     *       \a finished signal was emitted
     */
    void *nativeHandle () const;

    /**
     * @brief peekData
     * Will return the data at \a offset with length \a count.
     * If there is not yet enough data a empty vector will be returned
     */
    std::vector<char> peekData ( off_t offset, size_t count ) const;

    /**
     * @brief url
     * Returns the request URL
     */
    Url url () const;

    /**
     * @brief setUrl
     * This will change the URL of the request.
     * \note calling this on a currently running request has no impact
     */
    void setUrl ( const Url & url );

    /**
     * @brief targetFilePath
     * Returns the target filename path
     */
    const zypp::Pathname & targetFilePath () const;

    /**
     * @brief contentType
     * Returns the content type as reported from the server
     * \note can only return a valid value if the download has started already
     */
    std::string contentType () const;

    /**
     * @brief downloadOffset
     * Returns the requested start offset
     */
    off_t downloadOffset () const;

    /**
     * @brief reportedByteCount
     * Returns the number of bytes that are reported from the backend as the full download size, those can
     * be 0 even when the download is already running.
     */
    off_t reportedByteCount   () const;

    /**
     * @brief expectedByteCount
     * Returns the expected byte count that was passed to the constructor, zero if
     * none was given
     */
    off_t expectedByteCount   () const;

    /**
     * @brief downloadedByteCount
     * Returns the number of already downloaded bytes as reported by the backend
     */
    off_t downloadedByteCount () const;

    /**
     * @brief setDigest
     * Set a \sa zypp::Digest that is updated when data is written, can be used to
     * validate the file contents with a checksum
     */
    void setDigest ( std::shared_ptr<zypp::Digest> dig );

    /**
     * @brief digest
     * Returns the currently used \sa zypp::Digest, null if non was set.
     */
    std::shared_ptr<zypp::Digest> digest () const;

    /**
     * @brief setExpectedChecksum
     * Enables automated checking of downloaded contents against a checksum.
     * Only makes a difference if a \sa zypp::Digest was with with \sa setDigest.
     *
     * \note expects checksum in byte NOT in string format
     */
    void setExpectedChecksum (std::vector<unsigned char> checksum );

    /**
     * @brief state
     * Returns the current state the \a HttpDownloadRequest is in
     */
    State state () const;

    /**
     * @brief error
     * Returns the last set Error
     */
    const HttpRequestError &error () const;

    /**
     * @brief extendedErrorString
     * In some cases curl can provide extended error informations collected at
     * runtime. In those cases its possible to query that information.
     */
    std::string extendedErrorString() const;

    /**
     * @brief hasError
     * Checks if there was a error with the request
     */
    bool hasError () const;

    bool addRequestHeader(std::string &&header );

    /**
     * @brief sigStarted
     * Signals that the dispatcher dequeued the request and actually starts downloading data
     */
    SignalProxy<void ( HttpDownloadRequest &req )> sigStarted  ();

    /**
     * @brief sigProgress
     * Signals if there was data read from the download
     */
    SignalProxy<void ( HttpDownloadRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow )> sigProgress ();

    /**
     * @brief sigFinished
     * Signals that the download finished.
     *
     * \note After this signal was emitted the Curl handle is reset,
     *       so all queries to it need to be done in a Slot connected to this signal
     */
    SignalProxy<void ( HttpDownloadRequest &req, const HttpRequestError &err)> sigFinished ( );

  private:
    friend class HttpRequestDispatcher;
    friend class HttpRequestDispatcherPrivate;
    ZYPP_DECLARE_PRIVATE( HttpDownloadRequest )
  };

}

#endif
