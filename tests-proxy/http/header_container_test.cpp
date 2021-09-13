#include "proxy/http/header.hpp"
#include <clocale>
#include <iostream>
#include <iterator>
#include <vector>

bool check_content_same(std::string prefix,
                        std::vector<proxy::http::header> &headers_vector,
                        proxy::http::header_container &headers) {
  int index = 0;
  for (proxy::http::header_container::iterator it = headers.begin();
       it != headers.end() && index < headers_vector.size(); it++, index++) {

    const proxy::http::header &h = *it;
    if (headers_vector[index].name != h.name ||
        headers_vector[index].value != h.value) {
      std::cerr << prefix << "Invalid header at position " << index
                << ": expected (" << headers_vector[index].name << ", "
                << headers_vector[index].value << ") got (" << h.name << ", "
                << h.value << ")!" << std::endl;
      return false;
    }
  }
  if (index != headers_vector.size()) {
    std::cerr << prefix << "Too many headers in container!" << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  std::vector<proxy::http::header> headers_vector{
      {"Set-Cookie", "abc"}, {"Set-cookie", "def"}, {"Cookie", "jkl"},
      {"BERRY", "XYZ"},     {"cookie", "mno"},     {"Set-Cookie", "ghi"}};

  proxy::http::header_container headers;
  std::copy(headers_vector.begin(), headers_vector.end(),
            std::back_inserter(headers));

  if (!check_content_same("Initial check: ", headers_vector, headers)) {
    return 1;
  }

  std::vector<proxy::http::header> set_cookie_vector{
      {"Set-Cookie", "abc"}, {"Set-cookie", "def"}, {"Set-Cookie", "ghi"}};

  int index = 0;
  for (proxy::http::header_container::name_iterator iterator =
           headers.find("set-cookie");
       iterator != headers.end(); iterator++, index++) {
    const proxy::http::header &h = *iterator;
    if (set_cookie_vector[index].name != h.name ||
        set_cookie_vector[index].value != h.value) {
      std::cerr << "Invalid header at position " << index << ": expected ("
                << set_cookie_vector[index].name << ", "
                << set_cookie_vector[index].value << ") got (" << h.name << ", "
                << h.value << ")!" << std::endl;
      return 1;
    }
  }
  if (index != set_cookie_vector.size()) {
    std::cerr << "Too many Set-Cookie headers in container!" << std::endl;
    return 1;
  }

  for (int i = 0; i < 2; i++) {
    if (i == 1) {
      headers.erase_all("berry");
      headers.push_back({"BERRY", "zYx"});
    }
    proxy::http::header_container::name_iterator berry_it =
        headers.find("berry");
    if (berry_it == headers.end()) {
      std::cerr << "Missing BERRY header!" << std::endl;
      return 1;
    }
    if (berry_it->value != (i == 0 ? "XYZ" : "zYx")) {
      std::cerr << "Invalid value for BERRY header, got " << berry_it->value
                << "!" << std::endl;
      return 1;
    }
  }

  if (headers.empty()) {
    std::cerr << "Headers shouldn't be yet empty!" << std::endl;
    return 1;
  }

  headers.erase_all("cookie");
  std::vector<proxy::http::header> values_without_cookie{
      {"Set-Cookie", "abc"},
      {"Set-cookie", "def"},
      {"Set-Cookie", "ghi"},
      {"BERRY", "zYx"},
  };

  if (!check_content_same("Without cookie: ", values_without_cookie, headers)) {
    return 1;
  }

  if (headers.empty()) {
    std::cerr << "Headers shouldn't be yet empty!" << std::endl;
    return 1;
  }

  headers.erase_all("set-cookie");
  std::vector<proxy::http::header> values_without_cookie_and_set_cookie{
      {"BERRY", "zYx"}};

  if (!check_content_same("Without cookie and set-cookie: ",
                          values_without_cookie_and_set_cookie, headers)) {
    return 1;
  }

  if (headers.empty()) {
    std::cerr << "Headers shouldn't be yet empty!" << std::endl;
    return 1;
  }

  headers.clear();

  if (!headers.empty()) {
    std::cerr << "Headers should be empty!" << std::endl;
    return 1;
  }

  return 0;
}
