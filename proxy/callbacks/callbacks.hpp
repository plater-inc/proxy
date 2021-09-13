#ifndef PROXY_CALLBACKS_CALLBACKS_HPP
#define PROXY_CALLBACKS_CALLBACKS_HPP

#include "proxy/http/request_pre_body.hpp"
#include "proxy/http/response_pre_body.hpp"
#include "proxy/http/trailer.hpp"
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>

namespace proxy {
namespace callbacks {

typedef unsigned long long connection_id;
typedef unsigned long long request_id;

// All the callbacks below that receive request_pre_body receive the same object
// during the whole request lifecycle.
// All the callbacks below that receive
// response_pre_body receive the same object during the whole request lifecycle
// with the exception of the 100 Continue response that is delivered in a
// separate response_pre_body object.
struct proxy_callbacks : private boost::noncopyable {
  virtual void on_ready(unsigned short port_number) {}

  virtual void async_on_connection(connection_id connection_id,
                                   BOOST_ASIO_MOVE_ARG(boost::function<void()>)
                                       callback) {
    callback();
  }

  // When we see connect method, we can:
  // - tunnel it (callback argument false)
  // - bump it (callback argument true)
  struct on_connect_method_params {
    connection_id connection_id;
    std::string &host;
    std::string &service;
    http::request_pre_body &request_pre_body;
    BOOST_ASIO_MOVE_ARG(boost::function<void(bool)>) callback;
  };
  virtual void async_on_connect_method(on_connect_method_params &params) {
    params.callback(false);
  }

  struct on_request_pre_body_params {
    connection_id connection_id;
    request_id request_id;
    bool ssl;
    std::string &host;
    http::request_pre_body &request_pre_body;
    http::response_pre_body &response_pre_body;
    bool &expect_body_continue_from_downstream;
    bool &expect_100_continue_from_upstream;
    http::trailer &request_chunked_trailer;
    http::trailer &response_chunked_trailer;

    http::body_length_representation request_body_length_representation;
    unsigned long long request_body_length;

    std::vector<boost::asio::const_buffer> &outgoing_downstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_downstream_buffers_strings;
    std::vector<boost::asio::const_buffer> &outgoing_upstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_upstream_buffers_strings;

    // Param to the callback is send_immediately - don't process body before
    // sending pre_body.
    BOOST_ASIO_MOVE_ARG(boost::function<void(bool)>) callback;
  };

  // This function is called when we have read request pre_body from downstream.
  // Based on the contents of outgoing_downstream_buffers and
  // outgoing_upstream_buffers when the callback is called appropriate action
  // will be taken (both buffers are empty when they are passed to this
  // function):
  // - first, if the request has body and we already read part of it,
  // async_on_request_body_some will be called, so be careful about how you
  // manipulate the buffers there to not drop the effect of what you do in
  // async_on_request_pre_body
  // - then, if the final outgoing_upstream_buffers are not empty, they will be
  // sent to upstream (connecting there first if needed), or
  // - else, if the outgoing_downstream_buffers are not empty, they will be sent
  // to downstream, or
  // - else, if both buffers are empty, we will read more from downstream - that
  // option allows to wait for the request body before making decision what to
  // do with the request (probably not a good idea for request without a body).
  //
  // The response_pre_body this function receives will be empty (as no
  // upstream response has been received yet). In case a response has to be
  // returned, the response_pre_body has to be populated here
  // (http_version_string, code, reason and often Content-Length header).
  //
  // Buffers that are output by this function are shallow, the lifecycle of the
  // data they are based on has to be managed manually (it has to live since the
  // data is written to wire). To aid in this:
  // - both request_pre_body and response_pre_body will live as long as needed
  // - any strings that are needed can be moved to
  // outgoing_downstream_buffers_strings and outgoing_upstream_buffers_strings -
  // they will be kept as long as needed.
  //
  // Examples:
  // 1) Respond with abc to requests to /abc
  // if (params.request_pre_body.uri == "/abc") {
  //   callbacks::write_200_response_with_length(
  //       params.request_pre_body, params.response_pre_body,
  //       std::make_unique<std::string>("abc"),
  //       params.outgoing_downstream_buffers,
  //       params.outgoing_downstream_buffers_strings);
  //   params.callback(false);
  // } else {
  //   proxy_callbacks::async_on_request_pre_body(params);
  // }
  //
  // Note that if these requests have body, we have to drop the body in
  // async_on_request_body_some using code like:
  // if (params.request_pre_body.uri != "/abc") {
  //   proxy_callbacks::async_on_request_body_some(params);
  // } else {
  //   params.callback();
  // }
  //
  // 2) Wait for the full body before sending anything for requests to /abc
  // (assuming these requests have to have body!)
  // if (params.request_pre_body.uri != "/abc") {
  //   proxy_callbacks::async_on_request_pre_body(params);
  // } else {
  //   params.callback();
  // }
  //
  // And then in async_on_request_body_some:
  // if (params.request_pre_body.uri != "/abc") {
  //   proxy_callbacks::async_on_request_body_some(params);
  // } else if (!params.request_has_more_body) {
  //   callbacks::write_200_response_with_length(
  //       params.request_pre_body, params.response_pre_body,
  //       std::make_unique<std::string>("abc"),
  //       params.outgoing_downstream_buffers,
  //       params.outgoing_downstream_buffers_strings);
  //   params.callback();
  // }
  virtual void async_on_request_pre_body(on_request_pre_body_params &params) {
    params.outgoing_upstream_buffers = params.request_pre_body.to_buffers();
    params.callback(false);
  };

  struct on_request_body_some_params {
    connection_id connection_id;
    request_id request_id;
    bool ssl;
    std::string &host;
    http::request_pre_body &request_pre_body;
    http::response_pre_body &response_pre_body;
    bool &expect_100_continue_from_upstream;
    http::trailer &request_chunked_trailer;
    http::trailer &response_chunked_trailer;
    bool request_has_more_body;
    std::string::iterator body_begin;
    std::string::iterator body_end;

    http::body_length_representation request_body_length_representation;
    unsigned long long request_body_length;

    std::vector<boost::asio::const_buffer> &outgoing_downstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_downstream_buffers_strings;
    std::vector<boost::asio::const_buffer> &outgoing_upstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_upstream_buffers_strings;
    BOOST_ASIO_MOVE_ARG(boost::function<void()>) callback;
  };

  // This function will be called when some of the request body has been read
  // from downstream (and one more time after if it has never been called before
  // with response_has_more_body being false - regardless if downstream sent a
  // request body or not). The only exception is the rejected 100 continue flow.
  // Based on the contents of outgoing_downstream_buffers and
  // outgoing_upstream_buffers when the callback is called appropriate action
  // will be taken:
  // - if the outgoing_upstream_buffers are not empty, they will be
  // sent to upstream (connecting there first if needed), or
  // - else, if the outgoing_downstream_buffers are not empty, they will be sent
  // to downstream, or
  // - else, if both buffers are empty, we will read more from downstream - that
  // option allows to wait for the full request body before making decision what
  // to do with the request (that's probably not a good idea for last part of
  // body).
  //
  // Note that this function may be called with the non empty buffers
  // that were effect of async_on_request_pre_body - pay attention to not drop
  // them (unless desired).
  //
  // The response_pre_body this function receives will be empty (as no upstream
  // response has been received yet). The response_chunked_trailer will be
  // empty as well.
  //
  // The body will not include the request_chunked_trailer bytes in the chunked
  // encoding case. They have to be added there manually.
  //
  // request_chunked_trailer will be set on the last piece of chunked request
  // when the request_has_more_body is false.
  //
  // Note that this function may be called with body_begin == body_end when it
  // is being called one extra time if it wasn't previously called with
  // request_has_more_body being false. This case happens also in case the
  // bytes would only represent the trailer.
  //
  // In case a response has to be returned, the response_pre_body has to be
  // populated here (http_version_string, code, reason and often Content-Length
  // header).
  //
  // Buffers that are output by this function are shallow, the lifecycle of the
  // data they are based on has to be managed manually (it has to live since the
  // data is written to wire). To aid in this:
  // - both request_pre_body and response_pre_body will live as long as needed
  // - any strings that are needed can be moved to
  // outgoing_downstream_buffers_strings and outgoing_upstream_buffers_strings -
  // they will be kept as long as needed.
  //
  // See examples for async_on_request_pre_body.
  virtual void async_on_request_body_some(on_request_body_some_params &params) {
    if (params.body_begin != params.body_end) {
      params.outgoing_upstream_buffers.emplace_back(
          &*params.body_begin, params.body_end - params.body_begin);
    }
    if (!params.request_has_more_body) {
      if (params.request_body_length_representation ==
          http::body_length_representation::chunked) {
        std::vector<boost::asio::const_buffer> append =
            params.request_chunked_trailer.to_buffers();
        params.outgoing_upstream_buffers.insert(
            params.outgoing_upstream_buffers.end(), append.begin(),
            append.end());
      }
    }
    params.callback();
  };

  struct on_response_pre_body_params {
    connection_id connection_id;
    request_id request_id;
    bool ssl;
    std::string &host;
    http::request_pre_body &request_pre_body;
    http::response_pre_body &response_pre_body;
    bool &expect_body_continue_from_downstream;

    // True if it is HEAD etc. so even though there is content length it doesn't
    // mean there will be body.
    bool response_body_forbidden;

    http::body_length_representation response_body_length_representation;
    unsigned long long response_body_length;

    std::vector<boost::asio::const_buffer> &outgoing_downstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_downstream_buffers_strings;

    // Param to the callback is send_immediately - don't process body before
    // sending pre_body.
    BOOST_ASIO_MOVE_ARG(boost::function<void(bool)>) callback;
  };

  // This function is called when we have read response pre_body from upstream.
  // Based on the contents of outgoing_downstream_buffers when the callback is
  // called appropriate action will be taken (buffer is empty when they
  // are passed to this function):
  // - first, if the response has body and we already read part of it,
  // async_on_response_body_some will be called, so be careful about how you
  // manipulate the buffers there to not drop the effect of what you do in
  // async_on_response_pre_body
  // - then, if the final outgoing_downstream_buffers are not empty, they will
  // be sent to downstream, or
  // - else, we will read more from upstream - that option allows to wait for
  // the response body before making decision what to do with the response
  // (probably not a good idea for response without a body).
  //
  // Buffers that are output by this function are shallow, the lifecycle of the
  // data they are based on has to be managed manually (it has to live since the
  // data is written to wire). To aid in this:
  // - response_pre_body will live as long as needed
  // - any strings that are needed can be moved to
  // outgoing_downstream_buffers_strings - they will be kept as long as needed.
  virtual void async_on_response_pre_body(on_response_pre_body_params &params) {
    params.outgoing_downstream_buffers = params.response_pre_body.to_buffers();
    params.callback(false);
  };

  struct on_response_body_some_params {
    connection_id connection_id;
    request_id request_id;
    bool ssl;
    std::string &host;
    http::request_pre_body &request_pre_body;
    http::response_pre_body &response_pre_body;
    http::trailer &response_chunked_trailer;

    // True if it is HEAD etc. so even though there is content length it doesn't
    // mean there will be body.
    bool response_body_forbidden;

    bool response_has_more_body;
    std::string::iterator body_begin;
    std::string::iterator body_end;

    http::body_length_representation response_body_length_representation;
    unsigned long long response_body_length;

    std::vector<boost::asio::const_buffer> &outgoing_downstream_buffers;
    std::vector<std::unique_ptr<std::string>>
        &outgoing_downstream_buffers_strings;
    BOOST_ASIO_MOVE_ARG(boost::function<void()>) callback;
  };

  // This function will be called when some of the response body has been read
  // from upstream (and one more time after if it has never been called before
  // with response_has_more_body being false - regardless if upstream sent a
  // response body or even if it was allowed or not). Based on the contents of
  // outgoing_downstream_buffers when the callback is called appropriate action
  // will be taken:
  // - if the outgoing_downstream_buffers are not empty, they will be sent
  // to downstream, or
  // - else, if we will read more from upstream - that option allows to wait for
  // the full response body before making decision what to do with the response
  // (that's probably not a good idea for last part of body).
  //
  // Note that this function may be called with the non empty buffers
  // that were effect of async_on_response_pre_body - pay attention to not drop
  // them (unless desired).
  //
  // The body will not include the response_chunked_trailer bytes in the chunked
  // encoding case. They have to be added there manually.
  //
  // response_chunked_trailer will be set on the last piece of chunked response
  // when the response_has_more_body is false.
  //
  // Note that this function may be called with body_begin == body_end when it
  // is being called one extra time if it wasn't previously called with
  // response_has_more_body being false. This case happens for infinite
  // responses, for scenrios when upstream abruptly terminated
  // connection and also in case the bytes would only represent the trailer.
  //
  // Buffers that are output by this function are shallow, the lifecycle of the
  // data they are based on has to be managed manually (it has to live since the
  // data is written to wire). To aid in this:
  // - response_pre_body will live as long as needed
  // - any strings that are needed can be moved to
  // outgoing_downstream_buffers_strings - they will be kept as long as needed.
  virtual void
  async_on_response_body_some(on_response_body_some_params &params) {
    if (params.body_begin != params.body_end) {
      params.outgoing_downstream_buffers.emplace_back(
          &*params.body_begin, params.body_end - params.body_begin);
    }
    if (!params.response_has_more_body) {
      if (params.response_body_length_representation ==
          http::body_length_representation::chunked) {
        std::vector<boost::asio::const_buffer> append =
            params.response_chunked_trailer.to_buffers();
        params.outgoing_downstream_buffers.insert(
            params.outgoing_downstream_buffers.end(), append.begin(),
            append.end());
      }
    }
    params.callback();
  };

  // This function will be called when request/response finishes, regardless of:
  // - if it finishes sucessfully or not
  // - if upstream was called or not
  // - if upstream or downstream terminated connection mid flight.
  virtual void
  async_on_response_finished(connection_id connection_id, request_id request_id,
                             BOOST_ASIO_MOVE_ARG(boost::function<void()>)
                                 callback) {
    callback();
  }

  // There is nothing to be done after, so no async_ and no callback parameter.
  virtual void on_connection_finished(connection_id connection_id) {}
};

} // namespace callbacks
} // namespace proxy

#endif // PROXY_CALLBACKS_CALLBACKS_HPP