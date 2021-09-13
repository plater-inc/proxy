#include "body_without_length_parser.hpp"

namespace proxy {
namespace http_parser {

void body_without_length_parser::reset() {
  chunk_.reset();
  chunk_parser_.reset();
}

} // namespace http_parser
} // namespace proxy
