#include "chunk.hpp"

namespace proxy {
namespace http {

void chunk::reset() { chunk_length = 0; }

} // namespace http
} // namespace proxy