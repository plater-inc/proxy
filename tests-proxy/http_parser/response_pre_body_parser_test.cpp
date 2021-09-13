#include "proxy/http_parser/response_pre_body_parser.hpp"
#include "proxy/util/utils.hpp"
#include <clocale>
#include <iostream>

bool check_content_same(proxy::http::response_pre_body &input,
                        proxy::http::response_pre_body &expected) {
  if (input.code != expected.code) {
    std::cerr << "Wrong code: expected " << expected.code << " got "
              << input.code << "!" << std::endl;
    return false;
  }
  if (input.http_version_string != expected.http_version_string) {
    std::cerr << "Wrong http version string: expected "
              << expected.http_version_string << " got "
              << input.http_version_string << "!" << std::endl;
    return false;
  }
  if (input.reason != expected.reason) {
    std::cerr << "Wrong uri: expected " << expected.reason << " got "
              << input.reason << "!" << std::endl;
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

  proxy::http_parser::response_pre_body_parser parser;
  proxy::http::response_pre_body response_pre_body;

  std::vector<std::string> input = {
      ("HTTP/1.1 200 OK\r\n"
       "Date: Thu, 02 Sep 2021 15:46:35 GMT\r\n"
       "Server:      mw2271.codfw.wmnet\r\n"
       "X-Content-Type-Options: nosniff      \r\n"),
      ("P3p: CP=\"See "
       "https://en.wikipedia.org/wiki/Special:CentralAutoLogin/P3P for more "
       "info.\"\r\n"
       "Content-Langu"),
      "age: \t\t\ten\r\n"
      "Vary: Accept-Encoding,Cookie,Authorization\t\r\n"
      "Last-Modified: Thu, 02 Sep 2021 15:46:32 GMT    \t    \r\n"
      "Content-Type: text/html;       charset=UTF-8\r\n\t      \r\n"
      "Age: 1114\r\n"
      "X-Cache: cp1089 miss, cp1087 hit/2199\r\n"
      "X-Cache-Status:",
      (" hit-front\r\n"
       "Server-Timing: "),
      "cache;desc=\"hit-front\", host;desc=\"cp1087\"\r\n"
      "Strict-Transport-Security:max-age=106384710;\r\n includeSubDomains; "
      "preload\r\n"
      "Report-To: { \"group\": \"wm_nel\", \"max_age\": 86400, \"endpoints\": "
      "[{ \"url\": "
      "\"https://intake-logging.wikimedia.org/v1/"
      "events?stream=w3c.reportingapi.network_error&schema_uri=/w3c/"
      "reportingapi/network_error/1.0.0\" }] }\r\n"
      "NEL: { \"report_to\":\r\n\t\"wm_nel\", \"max_age\": 86400, "
      "\"failure_fr",
      ("action\": 0.05, \r\n \"success_fraction\": 0.0}\r\n"
       "Permissions-Policy: interest-cohort=()\r\n"
       "X-Client-IP: 162.250.131.198\r\n"
       "Cache-Control: priva"),
      ("te, s-maxage=0, max-age=0, must-revalidate\r\n"
       "Accept-Ranges: bytes\r\n"
       "Content-Length: 80764\r\n"
       "Connection: keep-alive\r"),
      ("\n"
       "\r\n")};

  proxy::http::response_pre_body expected{
      .http_version_string = "HTTP/1.1",
      .code = "200",
      .reason = "OK",
  };

  expected.headers.push_back({"Date", "Thu, 02 Sep 2021 15:46:35 GMT"});
  expected.headers.push_back({"Server", "mw2271.codfw.wmnet"});
  expected.headers.push_back({"X-Content-Type-Options", "nosniff"});
  expected.headers.push_back(
      {"P3p",
       "CP=\"See https://en.wikipedia.org/wiki/Special:CentralAutoLogin/P3P "
       "for more info.\""});
  expected.headers.push_back({"Content-Language", "en"});
  expected.headers.push_back({"Vary", "Accept-Encoding,Cookie,Authorization"});
  expected.headers.push_back(
      {"Last-Modified", "Thu, 02 Sep 2021 15:46:32 GMT"});
  expected.headers.push_back({"Content-Type", "text/html; charset=UTF-8"});
  expected.headers.push_back({"Age", "1114"});
  expected.headers.push_back({"X-Cache", "cp1089 miss, cp1087 hit/2199"});
  expected.headers.push_back({"X-Cache-Status", "hit-front"});
  expected.headers.push_back(
      {"Server-Timing", "cache;desc=\"hit-front\", host;desc=\"cp1087\""});
  expected.headers.push_back({"Strict-Transport-Security",
                              "max-age=106384710; includeSubDomains; preload"});
  expected.headers.push_back(
      {"Report-To", "{ \"group\": \"wm_nel\", \"max_age\": 86400, "
                    "\"endpoints\": [{ \"url\": "
                    "\"https://intake-logging.wikimedia.org/v1/"
                    "events?stream=w3c.reportingapi.network_error&schema_uri=/"
                    "w3c/reportingapi/network_error/1.0.0\" }] }"});
  expected.headers.push_back(
      {"NEL", "{ \"report_to\": \"wm_nel\", \"max_age\": 86400, "
              "\"failure_fraction\": 0.05, \"success_fraction\": 0.0}"});
  expected.headers.push_back({"Permissions-Policy", "interest-cohort=()"});
  expected.headers.push_back({"X-Client-IP", "162.250.131.198"});
  expected.headers.push_back(
      {"Cache-Control", "private, s-maxage=0, max-age=0, must-revalidate"});
  expected.headers.push_back({"Accept-Ranges", "bytes"});
  expected.headers.push_back({"Content-Length", "80764"});
  expected.headers.push_back({"Connection", "keep-alive"});

  for (int i = 0; i < 2; i++) {
    if (i == 1) {
      parser.reset();
    }
    response_pre_body = {};
    for (int j = 0; j < input.size(); j++) {
      boost::tribool result;
      std::string::iterator it = input[j].begin();

      boost::tie(result, it) =
          parser.parse(response_pre_body, it, input[j].end());

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
    if (!check_content_same(response_pre_body, expected)) {
      return 1;
    }
  }

  std::string expected_output =
      "HTTP/1.1 200 OK\r\n"
      "Date: Thu, 02 Sep 2021 15:46:35 GMT\r\n"
      "Server: mw2271.codfw.wmnet\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "P3p: CP=\"See "
      "https://en.wikipedia.org/wiki/Special:CentralAutoLogin/P3P for more "
      "info.\"\r\n"
      "Content-Language: en\r\n"
      "Vary: Accept-Encoding,Cookie,Authorization\r\n"
      "Last-Modified: Thu, 02 Sep 2021 15:46:32 GMT\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "Age: 1114\r\n"
      "X-Cache: cp1089 miss, cp1087 hit/2199\r\n"
      "X-Cache-Status: hit-front\r\n"
      "Server-Timing: cache;desc=\"hit-front\", host;desc=\"cp1087\"\r\n"
      "Strict-Transport-Security: max-age=106384710; includeSubDomains; "
      "preload\r\n"
      "Report-To: { \"group\": \"wm_nel\", \"max_age\": 86400, \"endpoints\": "
      "[{ \"url\": "
      "\"https://intake-logging.wikimedia.org/v1/"
      "events?stream=w3c.reportingapi.network_error&schema_uri=/w3c/"
      "reportingapi/network_error/1.0.0\" }] }\r\n"
      "NEL: { \"report_to\": \"wm_nel\", \"max_age\": 86400, "
      "\"failure_fraction\": 0.05, \"success_fraction\": 0.0}\r\n"
      "Permissions-Policy: interest-cohort=()\r\n"
      "X-Client-IP: 162.250.131.198\r\n"
      "Cache-Control: private, s-maxage=0, max-age=0, must-revalidate\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 80764\r\n"
      "Connection: keep-alive\r\n"
      "\r\n";
  if (proxy::util::utils::vector_of_buffers_to_string(
          response_pre_body.to_buffers()) != expected_output) {
    std::cerr << "Output doesn't match expected output!" << std::endl;
    return 1;
  }

  return 0;
}
