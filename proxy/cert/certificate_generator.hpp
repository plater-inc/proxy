#ifndef PROXY_CERT_GENERATE_CERTIFICATE_HPP
#define PROXY_CERT_GENERATE_CERTIFICATE_HPP

#include <boost/asio/ssl.hpp>
#include <boost/tuple/tuple.hpp>
#include <iostream>
#include <string>

namespace proxy {
namespace cert {

boost::tuple<std::string, std::string> generate_root_certificate();

boost::tuple<std::string, std::string>
generate_certificate(std::string host, std::string root_private_key,
                     std::string root_cert);

} // namespace cert
} // namespace proxy

#endif // PROXY_CERT_GENERATE_CERTIFICATE_HPP