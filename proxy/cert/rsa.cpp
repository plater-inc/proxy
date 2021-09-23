
#include "rsa.hpp"

namespace proxy {
namespace cert {

typedef std::unique_ptr<BIGNUM, decltype(&BN_free)> BIGNUM_ptr;

RSA_ptr generate_rsa() {
  BIGNUM_ptr bn(BN_new(), BN_free);
  if (!bn) {
    return RSA_ptr(nullptr, RSA_free);
  }
  if (!BN_set_word(bn.get(), RSA_F4)) {
    return RSA_ptr(nullptr, RSA_free);
  }
  RSA_ptr rsa(RSA_new(), RSA_free);
  if (!rsa) {
    return rsa; // in this case same as RSA_ptr(nullptr, RSA_free);
  }
  if (!RSA_generate_key_ex(rsa.get(), 2048, bn.get(), nullptr)) {
    return RSA_ptr(nullptr, RSA_free);
  }
  return rsa;
}

} // namespace cert
} // namespace proxy
