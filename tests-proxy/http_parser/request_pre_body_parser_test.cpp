#include "proxy/http_parser/request_pre_body_parser.hpp"
#include "proxy/util/utils.hpp"
#include <clocale>
#include <iostream>

bool check_content_same(proxy::http::request_pre_body &input,
                        proxy::http::request_pre_body &expected) {

  if (input.method != expected.method) {
    std::cerr << "Wrong method: expected " << expected.method << " got "
              << input.method << "!" << std::endl;
    return false;
  }
  if (input.http_version_string != expected.http_version_string) {
    std::cerr << "Wrong http version string: expected "
              << expected.http_version_string << " got "
              << input.http_version_string << "!" << std::endl;
    return false;
  }
  if (input.uri != expected.uri) {
    std::cerr << "Wrong uri: expected " << expected.uri << " got " << input.uri
              << "!" << std::endl;
    return false;
  }
  int index = 0;
  proxy::http::header_container::iterator input_it = input.headers.begin();
  proxy::http::header_container::iterator expected_it =
      expected.headers.begin();
  for (;
       input_it != input.headers.end() && expected_it != expected.headers.end();
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
  if (input_it != input.headers.end() ||
      expected_it != expected.headers.end()) {
    std::cerr << "Wrong number of headers in container!" << std::endl;
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::http_parser::request_pre_body_parser parser;
  proxy::http::request_pre_body request_pre_body;

  std::vector<std::string> input = {
      "GET /wiki/Main_Page HTTP/1.1\r\n"
      "Host:      en.wikipedia.org\r\n"
      "Connection: keep-alive      \r\n"
      "Pragma: \t\t\t\tno-cache\r\n"
      "Cache-Control: no-cache\t\r\n"
      "accept: "
      "text/html,application/xhtml+xml,",
      ("application/xml;q=0.9,image/avif,image/"
       "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
       "upgrade-insecure-requests: 1\r\n"
       "User-Agent: "),
      "Mozilla/5.0    (Macintosh; Intel Mac OS X 11_0_0) "
      "AppleWebKit/537.36 (KHTML,\tlike\t \tGecko) Chrome/88.0.4298.0 "
      "Safari/537.36\r\n"
      "Sec-Fetch-Site:same-origin\r\n    \t   \r\n"
      "Sec-Fetch-Mode:same-origin     \t     \r\n"
      "Sec-Fetch-Dest: empty\r\n"
      "Referer: https://en.wikipedia.org/\r\n",
      ("Accept-Encoding: gzip,\r\n deflate, br\r\n"
       "Accept-Language"),
      ": en-US,en;q=0.9\r\n"
      "Cookie:\r\n\t   GeoIP=US:GA:Proxyville:12.34:-45.67:v4; "
      "GeoIP=US:CA:Proxyville:12.34:-45.67:v4; "
      "enwikimwuser-sessionId=g3i4ogj3o4if; "
      "WMF-Last-Access=03-Sep-2021; \r\n\tWMF-Last-Access-Global=03-Sep-2021; "
      "enwikiel-sessionId=fm34oimfg43; "
      "enwikiwmE-sessionTickLastTickTime=1630678318600; "
      "enwikiwmE-sessionTickTickCount=36\r\n"
      "\r\n"};

  proxy::http::request_pre_body expected{
      .method = "GET",
      .uri = "/wiki/Main_Page",
      .http_version_string = "HTTP/1.1",
  };
  expected.headers.push_back({"Host", "en.wikipedia.org"});
  expected.headers.push_back({"Connection", "keep-alive"});
  expected.headers.push_back({"Pragma", "no-cache"});
  expected.headers.push_back({"Cache-Control", "no-cache"});
  expected.headers.push_back(
      {"accept",
       "text/html,application/xhtml+xml,application/xml;q=0.9,image/"
       "avif,image/"
       "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9"});
  expected.headers.push_back({"upgrade-insecure-requests", "1"});
  expected.headers.push_back(
      {"User-Agent",
       "Mozilla/5.0 (Macintosh; Intel Mac OS X 11_0_0) "
       "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4298.0 "
       "Safari/537.36"});
  expected.headers.push_back({"Sec-Fetch-Site", "same-origin"});
  expected.headers.push_back({"Sec-Fetch-Mode", "same-origin"});
  expected.headers.push_back({"Sec-Fetch-Dest", "empty"});
  expected.headers.push_back({"Referer", "https://en.wikipedia.org/"});
  expected.headers.push_back({"Accept-Encoding", "gzip, deflate, br"});
  expected.headers.push_back({"Accept-Language", "en-US,en;q=0.9"});
  expected.headers.push_back(
      {"Cookie",
       "GeoIP=US:GA:Proxyville:12.34:-45.67:v4; "
       "GeoIP=US:CA:Proxyville:12.34:-45.67:v4; "
       "enwikimwuser-sessionId=g3i4ogj3o4if; "
       "WMF-Last-Access=03-Sep-2021; WMF-Last-Access-Global=03-Sep-2021; "
       "enwikiel-sessionId=fm34oimfg43; "
       "enwikiwmE-sessionTickLastTickTime=1630678318600; "
       "enwikiwmE-sessionTickTickCount=36"});
  for (int i = 0; i < 2; i++) {
    if (i == 1) {
      parser.reset();
    }
    request_pre_body = {};
    for (int j = 0; j < input.size(); j++) {
      boost::tribool result;
      std::string::iterator it = input[j].begin();

      boost::tie(result, it) =
          parser.parse(request_pre_body, it, input[j].end());

      if ((j == (input.size() - 1) && result != true) ||
          (j < (input.size() - 1) && result != boost::indeterminate)) {
        std::cerr << "Failed parsing pre body piece " << j << " at position "
                  << (it - input[j].begin()) << std::endl;
        return 1;
      }
      if (it != input[j].end()) {
        std::cerr << "Not whole input digested in piece " << j
                  << " at position " << (it - input[j].begin()) << std::endl;
        return 1;
      }
    }
    if (!check_content_same(request_pre_body, expected)) {
      return 1;
    }
  }

  std::string expected_output =
      "GET /wiki/Main_Page HTTP/1.1\r\n"
      "Host: en.wikipedia.org\r\n"
      "Connection: keep-alive\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
      "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
      "upgrade-insecure-requests: 1\r\n"
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 11_0_0) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4298.0 "
      "Safari/537.36\r\n"
      "Sec-Fetch-Site: same-origin\r\n"
      "Sec-Fetch-Mode: same-origin\r\n"
      "Sec-Fetch-Dest: empty\r\n"
      "Referer: https://en.wikipedia.org/\r\n"
      "Accept-Encoding: gzip, deflate, br\r\n"
      "Accept-Language: en-US,en;q=0.9\r\n"
      "Cookie: GeoIP=US:GA:Proxyville:12.34:-45.67:v4; "
      "GeoIP=US:CA:Proxyville:12.34:-45.67:v4; "
      "enwikimwuser-sessionId=g3i4ogj3o4if; "
      "WMF-Last-Access=03-Sep-2021; WMF-Last-Access-Global=03-Sep-2021; "
      "enwikiel-sessionId=fm34oimfg43; "
      "enwikiwmE-sessionTickLastTickTime=1630678318600; "
      "enwikiwmE-sessionTickTickCount=36\r\n"
      "\r\n";

  if (proxy::util::utils::vector_of_buffers_to_string(
          request_pre_body.to_buffers()) != expected_output) {
    std::cerr << "Output doesn't match expected output!" << std::endl;
    return 1;
  }

  return 0;
}
