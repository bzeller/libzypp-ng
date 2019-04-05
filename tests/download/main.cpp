#include <iostream>
#include <media/curl/curl.h>
#include <media/curl/request.h>
#include <zypp/Digest.h>
#include <boost/asio/io_context.hpp>

int main ( int , char *[] )
{

  std::cout << "Starting up " << std::endl;

  boost::asio::io_context ios;

  std::string baseURL = "https://download.opensuse.org/distribution/leap/15.0/repo/oss/x86_64/";
  std::vector<std::string> downloads {
    "11822f1421ae50fb1a07f72220b79000", "0ad-0.0.22-lp150.2.10.x86_64.rpm",
    "b0aaaca4c3763792a495de293c8431c5", "alsa-1.1.5-lp150.4.3.x86_64.rpm",
    "a6eb92351c03bcf603a09a2e8eddcead", "amarok-2.9.0-lp150.2.1.x86_64.rpm",
    "a2fd84f6d0530abbfe6d5a3da3940d70", "aspell-0.60.6.1-lp150.1.15.x86_64.rpm",
    "29b5eab4a9620f158331106df4603866", "atk-devel-2.26.1-lp150.2.4.x86_64.rpm",
    "a795874986018674c37af85a62c8f28a", "bing-1.0.5-lp150.2.1.x86_64.rpm",
    "ce09cb1af156203c89312f9faffce219", "cdrtools-3.02~a09-lp150.2.11.x86_64.rpm",
    "3f57113bd0dea2b5d748b7a343a7fb31", "cfengine-3.11.0-lp150.2.3.x86_64.rpm",
    "582cae086f67382e71bf02ff0db554cd", "cgit-1.1-lp150.1.5.x86_64.rpm",
    "f10cfc37a20c13d59266241ecc30b152", "ck-devel-0.6.0-lp150.1.3.x86_64.rpm",
    "4a19708dc8d58129f8832d934c47888b", "cloud-init-18.2-lp150.1.1.x86_64.rpm",
    "7b535587d9bfd8b88edf57b6df5c4d99", "collectd-plugin-pinba-5.7.2-lp150.1.4.x86_64.rpm",
    "d86c1d65039645a1895f458f38d9d9e7", "compface-1.5.2-lp150.1.4.x86_64.rpm",
    "33a0e878c92b5b298cd6aaec44c0aa46", "compositeproto-devel-0.4.2-lp150.1.6.x86_64.rpm",
    "646c6cc180caf27f56bb9ec5b4d50f5b", "corosync-testagents-2.4.4-lp150.3.1.x86_64.rpm",
    "10685e733abf77e7439e33471b23612c", "cpupower-bench-4.15-lp150.1.4.x86_64.rpm"
  };

  zyppng::HttpRequestDispatcher downloader( "/tmp/test" );

  assert ( downloads.size() % 2 == 0 );
  for ( int i = 0; i < downloads.size(); i+=2 ) {
    const std::string &checksum = downloads[i];
    const std::string &filename = downloads[i+1];

    zypp::Url url ( baseURL );
    url.appendPathName( filename );

    off_t start = -1;
    off_t len = 0;
    if ( i == 0 ) {
      start = 0;
      len = 256;
    }

    std::shared_ptr<zyppng::HttpDownloadRequest> req = std::make_shared<zyppng::HttpDownloadRequest>( zypp::Url(url), "/tmp/test", start, len );

    std::shared_ptr<zypp::Digest> dig = std::make_shared<zypp::Digest>();
    if ( !dig->create( zypp::Digest::md5() ) ) {
      std::cerr << "Unable to create Digest " << std::endl;
      return 1;
    }
    req->setDigest( dig );
    req->setExpectedChecksum( checksum );

    req->sigStarted().connect( [] ( const zyppng::HttpDownloadRequest &r ){
      std::cout << r.url() << " started downloading " << std::endl;
    });
    req->sigFinished().connect( [] ( const zyppng::HttpDownloadRequest &r, const zyppng::HttpRequestError &err ){
      if ( err.isError() ) {
        std::cout << r.url() << " finsihed with err " << err.nativeCode() << " : "<< err.isError() << " : " << err.toString() << std::endl;
      } else {
        std::cout << r.url() << " finished downloading " << std::endl;
      }
    });
#if 0
    req->sigProgress().connect( [] ( const zyppng::CurlDownloaderRequest &r, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow ){
      if ( dltotal == 0 && ultotal == 0 )
        return;

      std::cout << r.url() << " at: " << std::endl
                << "dltotal: "<< dltotal<< std::endl
                << "dlnow: "<< dlnow<< std::endl
                << "ultotal: "<< ultotal<< std::endl
                << "ulnow: "<< ulnow<< std::endl;
    });
#endif

    downloader.enqueue( req );

  }


  downloader.run( &ios );
  ios.run();
  return 0;
}
