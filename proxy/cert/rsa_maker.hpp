#ifndef PROXY_CERT_RSA_MAKER_HPP
#define PROXY_CERT_RSA_MAKER_HPP

#include "rsa.hpp"
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <vector>

namespace proxy {
namespace cert {

class rsa_maker : private boost::noncopyable {
  static const size_t SOFT_LIMIT = 128;
  static const size_t HARD_LIMIT = 156;
  std::vector<RSA_ptr> storage;
  size_t count = 0;

public:
  rsa_maker();
  typedef boost::function<void()> reschedule;
  void generate_callback(reschedule reschedule);
  RSA_ptr get(reschedule reschedule);
};

} // namespace cert
} // namespace proxy

#endif // PROXY_CERT_RSA_MAKER_HPP
