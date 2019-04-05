#include <media/curl/httprequesterror.h>
#include <curl/curl.h>

zyppng::HttpRequestError::HttpRequestError(zyppng::HttpRequestError::Type t_r, int nativeCode_r, std::string err_r)
  : _type( t_r )
  , _nativeCode( nativeCode_r )
, _error (std::move(err_r) )
{
  const char *nativeErr = nullptr;
  if ( t_r == CurlMError && err_r.empty() ) {
    nativeErr = curl_multi_strerror( static_cast<CURLMcode>(nativeCode_r) );
  } else if ( t_r == CurlError && err_r.empty() ) {
    nativeErr = curl_easy_strerror( static_cast<CURLcode>(nativeCode_r) );
  }
  if ( nativeErr != nullptr )
    _error = std::string( nativeErr );
}

zyppng::HttpRequestError::HttpRequestError(zyppng::HttpRequestError::Type t, int nativeCode, const char *err)
  : HttpRequestError( t, nativeCode, err != nullptr ? std::string( err ) : std::string() )
{ }

int zyppng::HttpRequestError::nativeCode() const
{
  return _nativeCode;
}

zyppng::HttpRequestError::Type zyppng::HttpRequestError::type() const
{
  return _type;
}

std::string zyppng::HttpRequestError::toString() const
{
  return _error;
}

bool zyppng::HttpRequestError::isError() const
{
  return _type != NoError;
}