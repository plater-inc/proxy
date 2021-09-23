#ifndef PROXY_CERT_GENERATE_CERTIFICATE_HPP
#define PROXY_CERT_GENERATE_CERTIFICATE_HPP

#include "rsa_maker.hpp"
#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>
#include <string>

namespace proxy {
namespace cert {

struct certificate_generator : private boost::noncopyable {
  certificate_generator(rsa_maker &rsa_maker);

  boost::tuple<std::string, std::string> generate_root_certificate();

  boost::tuple<std::string, std::string>
  generate_certificate(std::string host, std::string root_private_key,
                       std::string root_cert, rsa_maker::reschedule reschedule);

private:
  rsa_maker &rsa_maker_;
};

} // namespace cert
} // namespace proxy

#endif // PROXY_CERT_GENERATE_CERTIFICATE_HPP