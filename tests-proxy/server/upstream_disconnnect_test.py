import test_util.proxy
import test_util.runner
import http.client
import http.server
import threading
import test_util.thread_safe_counter
import random

if __name__ == "__main__":

    request_counter = test_util.thread_safe_counter.Counter()

    # This is HTTP 1.1 server that does support persisent connections but we
    # close it.
    class Server(http.server.BaseHTTPRequestHandler):

        protocol_version = 'HTTP/1.1'

        disable_nagle_algorithm = True

        def do_GET(self):
            request_counter.increment()
            if self.path == "/abc/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                response = b"<h1>abc</h1>"
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
                self.close_connection = True

    server = http.server.HTTPServer(('localhost', 0), Server)
    server_port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever)
    thread.daemon = True  # thread dies with the program
    thread.start()

    expected_request_counter = 0

    def test_request(suffix):
        global expected_request_counter

        url = "http://localhost:%d/%s/" % (server_port, suffix)
        http_connection.request("GET", url)
        if suffix == "abc":
            expected_request_counter += 1
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_pre_body /%s/\n" % suffix)
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_body_some_last /%s/\n" % suffix)
        if suffix == "abc":
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body /%s/\n" % suffix)
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /%s/\n" % suffix)
        test_util.runner.get_line_from_queue_and_assert(queue,
                                                        "response_finished\n")

        response = http_connection.getresponse()
        expected_body = b"<h1>%s</h1>" % str.encode(suffix)
        read_body = response.read()
        assert read_body == expected_body, ("%s body %s doesn't match %s!" %
                                            (url, read_body, expected_body))

        assert expected_request_counter == request_counter.value(), (
            "Unexpected request_count - expected %d was %d",
            expected_request_counter, request_counter.value())

    queue, proxy_process = test_util.runner.run(
        "./tests-proxy/server/upstream_disconnnect_test_proxy")

    proxy_port = int(queue.get().strip())

    http_connection = http.client.HTTPConnection("127.0.0.1", proxy_port)
    http_connection.connect()
    test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")

    if random.randint(0, 1) == 0:
        test_request("def")
    test_request("abc")

    server.shutdown()

    test_request("def")

    http_connection.close()

    test_util.runner.get_line_from_queue_and_assert(queue,
                                                    "connection_finished\n")

    proxy_process.kill()
