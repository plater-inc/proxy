#include "body_with_length_parser.hpp"

namespace proxy {
namespace http_parser {

void body_with_length_parser::reset(unsigned long long length) {
  remaining_body_count_ = length;
}

} // namespace http_parser
} // namespace proxy
