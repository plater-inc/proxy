#ifndef PROXY_CERT_RSA_HPP
#define PROXY_CERT_RSA_HPP

#include <boost/asio/ssl.hpp>

namespace proxy {
namespace cert {

typedef std::unique_ptr<RSA, decltype(&RSA_free)> RSA_ptr;

RSA_ptr generate_rsa();

} // namespace cert
} // namespace proxy

#endif // PROXY_CERT_RSA_HPP
