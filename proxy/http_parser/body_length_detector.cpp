#include "body_length_detector.hpp"
#include "proxy/http/header.hpp"
#include "proxy/util/misc_strings.hpp"
#include "proxy/util/utils.hpp"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <vector>

namespace proxy {
namespace http_parser {

const unsigned long long MAX_LENGTH_BEFORE_LAST_DIGIT = (ULLONG_MAX - 9) / 10;
const std::string CHUNKED = "chunked";

[[nodiscard]] boost::tuple<bool, http::body_length_representation,
                           unsigned long long>
detect_body_length(http::header_container &headers) {
  for (http::header_container::name_iterator transfer_encoding_it =
           headers.find("transfer-encoding");
       transfer_encoding_it != headers.end(); transfer_encoding_it++) {
    const http::header &h = *transfer_encoding_it;
    if (h.value.length() >= CHUNKED.length()) {
      if (h.value.compare(h.value.length() - CHUNKED.length(), CHUNKED.length(),
                          CHUNKED) == 0) {
        return boost::make_tuple(true,
                                 http::body_length_representation::chunked,
                                 0ULL /* ignored */);
      }
    }
  }

  http::header_container::name_iterator content_length_it =
      headers.find("content-length");
  if (content_length_it != headers.end()) {
    unsigned long long length = 0ULL;
    const http::header &h = *content_length_it;
    for (int i = 0; i < h.value.length(); i++) {
      if (length <= MAX_LENGTH_BEFORE_LAST_DIGIT) {
        int digit = util::misc_strings::digit_to_int_safe(h.value[i]);
        if (digit > -1) {
          length = length * 10 + digit;
        } else {
          return boost::make_tuple(
              false, http::body_length_representation::none /* ignored */,
              0ULL /* ignored */);
        }
      } else {
        return boost::make_tuple(
            false, http::body_length_representation::none /* ignored */,
            0ULL /* ignored */);
      }
    }
    return boost::make_tuple(
        true, http::body_length_representation::content_length, length);
  }

  return boost::make_tuple(true, http::body_length_representation::none,
                           0ULL /* ignored */);
}

} // namespace http_parser
} // namespace proxy