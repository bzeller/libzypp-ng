#ifndef ZYPP_NG_MEDIA_CURL_HTTP_REQUEST_ERROR_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_HTTP_REQUEST_ERROR_H_INCLUDED

#include <core/zyppglobal.h>
#include <string>

namespace zyppng {

  /**
   * @brief The HttpRequestError class
   * Represents a error that occured in \see HttpDownloadRequest
   * or \see HttpRequestDispatcher
   */
  class LIBZYPP_NG_EXPORT HttpRequestError
  {
  public:
    enum Type {
      NoError,
      Cancelled,
      CurlMError,
      CurlError,
      ExceededMaxLen,
      InvalidChecksum
    };

    HttpRequestError () = default;
    HttpRequestError ( Type t_r, int nativeCode_r , std::string err_r = std::string() );
    HttpRequestError ( Type t, int nativeCode, const char *err );

    /**
     * @brief nativeCode
     * This returns the native error code if available,
     * this is a CURLMCode for \a CurlMError or a CURLCode for \a CurlError.
     * If no native code is available -1 is returned
     */
    int nativeCode () const;

    /**
     * @brief type
     * Returns the type of the error
     */
    Type type () const;

    /**
     * @brief toString
     * Returns a string representation of the error
     */
    std::string toString () const;

    /**
     * @brief isError
     * Will return true if this is a actual error
     */
    bool isError () const;

  private:
    Type _type = NoError;
    int _nativeCode = -1;
    std::string _error;

  };
}

#endif
