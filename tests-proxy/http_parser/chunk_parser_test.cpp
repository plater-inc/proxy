#include "proxy/http_parser/chunk_parser.hpp"
#include "proxy/util/utils.hpp"
#include <clocale>
#include <iostream>

bool check_content_same(
    proxy::http::chunk &input_chunk, proxy::http::chunk &expected_chunk,
    proxy::http::trailer &input_trailer,
    std::string &expected_trailer_extension,
    std::vector<proxy::http::header> &expected_trailer_headers) {

  if (input_chunk.chunk_length != expected_chunk.chunk_length) {
    std::cerr << "Wrong extension: expected " << expected_chunk.chunk_length
              << " got " << input_chunk.chunk_length << "!" << std::endl;
    return false;
  }
  if (input_trailer.extension != expected_trailer_extension) {
    std::cerr << "Wrong extension: expected " << expected_trailer_extension
              << " got " << input_trailer.extension << "!" << std::endl;
    return false;
  }
  int index = 0;
  proxy::http::header_container::iterator input_it =
      input_trailer.headers.begin();
  std::vector<proxy::http::header>::iterator expected_it =
      expected_trailer_headers.begin();
  for (; input_it != input_trailer.headers.end() &&
         expected_it != expected_trailer_headers.end();
       input_it++, expected_it++, index++) {
    if (input_it->name != expected_it->name ||
        input_it->value != expected_it->value) {
      std::cerr << "Invalid header at position " << index << ": expected ("
                << expected_it->name << ", " << expected_it->value << ") got ("
                << input_it->name << ", " << input_it->value << ")!"
                << std::endl;
      return false;
    }
  }
  if (input_it != input_trailer.headers.end() ||
      expected_it != expected_trailer_headers.end()) {
    std::cerr << "Wrong number of headers in container!" << std::endl;
    return false;
  }

  return true;
}

bool test(std::vector<std::string> input, proxy::http::chunk expected_chunk,
          std::string expected_trailer_extension,
          std::vector<proxy::http::header> expected_trailer_headers,
          std::string expected_trailer_output) {
  proxy::http_parser::chunk_parser parser;
  proxy::http::chunk chunk;
  proxy::http::trailer trailer;
  for (int i = 0; i < 2; i++) {
    if (i == 1) {
      parser.reset();
    }
    chunk = {};
    trailer = {};
    for (int j = 0; j < input.size(); j++) {
      boost::tribool result;
      std::string::iterator it = input[j].begin();

      boost::tie(result, it) = parser.parse(chunk, trailer, it, input[j].end());

      if ((j == (input.size() - 1) && result != true) ||
          (j < (input.size() - 1) && result != boost::indeterminate)) {
        std::cerr << "Failed parsing chunk piece " << j << " at position "
                  << (it - input[j].begin()) << std::endl;
        return false;
      }
      if (it != input[j].end()) {
        std::cerr << "Not whole input digested in piece " << j
                  << " at position " << (it - input[j].begin()) << std::endl;
        return false;
      }
    }
    if (!check_content_same(chunk, expected_chunk, trailer,
                            expected_trailer_extension,
                            expected_trailer_headers)) {
      return false;
    }
  }

  if (proxy::util::utils::vector_of_buffers_to_string(trailer.to_buffers()) !=
      expected_trailer_output) {
    std::cerr << "Output doesn't match expected output!" << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  if (!test({"5\r\n", "afdbf\r\n"}, {.chunk_length = 5}, "", {}, "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"0000005\r\n", "afdbf\r\n"}, {.chunk_length = 5}, "", {},
            "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"5;abc\r\n", "afdbf\r\n"}, {.chunk_length = 5}, "", {},
            "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"5", ";abc\r\n", "afdbf\r\n"}, {.chunk_length = 5}, "", {},
            "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"0\r\n", "\r\n"}, {.chunk_length = 0}, "", {}, "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"0", "0", "\r\n\r\n"}, {.chunk_length = 0}, "", {}, "0\r\n\r\n")) {
    return 1;
  }

  if (!test({"0\r\n",
             "A: B\r\n"
             "Cfmfo4i3jf34: g43iogjmo34kmfg\r\n"
             " m4oi3gfm2",
             "4o4gm2\t \r\n"
             "m44gf43i3:\tmfo24igfm2og\t"
             "    \tfm42oifgm24iog\r\n"
             "g3i4ogmw3ogmk:      3m4ogi3mo4\t\r\n"
             "\r\n"},
            {.chunk_length = 0}, "",
            {{"A", "B"},
             {"Cfmfo4i3jf34", "g43iogjmo34kmfg m4oi3gfm24o4gm2"},
             {"m44gf43i3", "mfo24igfm2og fm42oifgm24iog"},
             {"g3i4ogmw3ogmk", "3m4ogi3mo4"}},
            "0\r\n"
            "A: B\r\n"
            "Cfmfo4i3jf34: g43iogjmo34kmfg m4oi3gfm24o4gm2\r\n"
            "m44gf43i3: mfo24igfm2og fm42oifgm24iog\r\n"
            "g3i4ogmw3ogmk: 3m4ogi3mo4\r\n"
            "\r\n")) {
    return 1;
  }

  if (!test({"0", ";abc", "\r\n\r\n"}, {.chunk_length = 0}, ";abc", {},
            "0;abc\r\n\r\n")) {
    return 1;
  }

  return 0;
}
