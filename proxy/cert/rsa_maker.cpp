
#include "rsa_maker.hpp"
#include "rsa.hpp"

namespace proxy {
namespace cert {

rsa_maker::rsa_maker() {
  storage.reserve(HARD_LIMIT);
  for (int i = 0; i < HARD_LIMIT; i++) {
    storage.push_back(RSA_ptr(nullptr, RSA_free));
  }
}

void rsa_maker::generate_callback(reschedule reschedule) {
  if (count >= HARD_LIMIT) {
    return;
  }
  storage[count++] = generate_rsa();
  if (count < SOFT_LIMIT) {
    reschedule();
  }
}

RSA_ptr rsa_maker::get(reschedule reschedule) {
  if (count <= SOFT_LIMIT) {
    reschedule();
  }
  if (count == 0) {
    return generate_rsa();
  } else {
    return RSA_ptr(storage[--count].release(), RSA_free);
  }
}

} // namespace cert
} // namespace proxy
