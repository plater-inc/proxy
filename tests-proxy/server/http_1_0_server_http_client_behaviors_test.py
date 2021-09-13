import queue
import test_util.proxy
import test_util.runner
import http.client
import http.server
import threading
import test_util.thread_safe_counter
import random
import time

if __name__ == "__main__":

    request_counter = test_util.thread_safe_counter.Counter()

    # This is HTTP 1.0 server that doesn't support persisent connections
    class Server(http.server.BaseHTTPRequestHandler):

        disable_nagle_algorithm = True

        def do_HEAD(self):
            request_counter.increment()
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.send_header("Content-Length", 5)
            self.end_headers()

        def write_chunked(self, what, padded):
            remaining = what
            while len(remaining) > 0:
                chunk_length = random.randint(1, len(remaining))
                self.write_chunk(remaining[:chunk_length], padded)
                remaining = remaining[chunk_length:]
            self.write_chunk(remaining, padded)

        def write_chunk(self, chunk, padded):
            if padded and random.randint(0, 1) == 0:
                self.wfile.write(b"0")
                self.wfile.flush()
                time.sleep(0.001)
            padding = random.randint(0, 5) if padded else 0
            self.wfile.write(((b"%%0%dx\r\n") % padding) % len(chunk) + chunk +
                             b"\r\n")
            self.wfile.flush()
            if random.randint(0, 1) == 0:
                time.sleep(0.001)

        def do_POST(self):
            request_counter.increment()
            content_length = int(self.headers['Content-Length'])
            body = self.rfile.read(content_length) if content_length > 0 else ""
            body_length = len(body)
            if self.path == "/body_200_no_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write(b"<h1>body_200 %d</h1>" % body_length)
            elif self.path == "/body_200_length/":
                self.send_response(200)
                response = b"<h1>body_200 %d</h1>" % body_length
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
            elif self.path == "/body_200_chunked/":
                self.send_response(200)
                response = b"<h1>body_200 %d</h1>" % body_length
                self.send_header("Content-type", "text/html")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                self.write_chunked(response, padded=False)
            elif self.path == "/body_200_chunked_padded_length/":
                self.send_response(200)
                response = b"<h1>body_200 %d</h1>" % body_length
                self.send_header("Content-type", "text/html")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                self.write_chunked(response, padded=True)
            elif self.path == "/no_body_200_no_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
            elif self.path == "/no_body_200_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", "0")
                self.end_headers()

        def do_GET(self):
            request_counter.increment()
            if self.path == "/body_200_no_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write(b"<h1>body_200</h1>")
            elif self.path == "/body_200_length/":
                self.send_response(200)
                response = b"<h1>body_200</h1>"
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
            elif self.path == "/body_200_chunked/":
                self.send_response(200)
                response = b"<h1>body_200</h1>"
                self.send_header("Content-type", "text/html")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                self.write_chunked(response, padded=False)
            elif self.path == "/body_200_chunked_padded_length/":
                self.send_response(200)
                response = b"<h1>body_200</h1>"
                self.send_header("Content-type", "text/html")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                self.write_chunked(response, padded=True)
            elif self.path == "/no_body_200_no_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
            elif self.path == "/no_body_200_length/":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", "0")
                self.end_headers()

    server = http.server.HTTPServer(('localhost', 0), Server)
    server_port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever)
    thread.daemon = True  # thread dies with the program
    thread.start()

    expected_request_counter = 0

    def test_requests(proxy_port, behavior, send_immediately, method, suffix):
        global expected_request_counter
        http_connection = http.client.HTTPConnection("127.0.0.1", proxy_port)
        http_connection.connect()
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")

        url = "http://localhost:%d/%s/" % (server_port, suffix)
        if method == "POST":
            # We want to often test short payloads that will fit in the same
            # packet with the pre body.
            body_length = random.choice(
                [random.randint(0, 1),
                 random.randint(2, 100000)])
            body = "a" * body_length
        else:
            body_length = None
            body = None
        http_connection.request(method, url, body)
        stream = "none" if behavior.startswith(
            "buffer_request_") or behavior in {
                "request_body_last_generates_response_with_body",
                "request_body_last_generates_response_without_body",
            } else ("downstream" if behavior in {
                "request_pre_body_generates_response_with_body",
                "request_pre_body_generates_response_without_body",
            } else "upstream")
        if behavior.endswith("default") or behavior.endswith(
                "response_pre_body_generates_response_with_body"
        ) or behavior.endswith(
                "response_pre_body_generates_response_without_body"
        ) or behavior.endswith(
                "response_pre_body_prepend") or behavior.endswith(
                    "response_body_prepend") or behavior.endswith(
                        "response_body_append"):
            expected_request_counter += 1
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_pre_body /%s/ %s\n" % (suffix, stream))
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_body_some_last /%s/\n" % suffix)
        if behavior.endswith("default") or behavior.endswith(
                "response_pre_body_generates_response_with_body"
        ) or behavior.endswith(
                "response_pre_body_generates_response_without_body"
        ) or behavior.endswith(
                "response_pre_body_prepend") or behavior.endswith(
                    "response_body_prepend") or behavior.endswith(
                        "response_body_append"):
            stream = "none" if False else "downstream"
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body /%s/ 200 %s\n" % (suffix, stream))
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /%s/\n" % suffix)
        test_util.runner.get_line_from_queue_and_assert(queue,
                                                        "response_finished\n")

        response = http_connection.getresponse()
        if method == "HEAD":
            expected_body = b""
        elif behavior in {
                "request_pre_body_generates_response_with_body",
                "request_body_last_generates_response_with_body",
        } or behavior.endswith(
                "response_pre_body_generates_response_with_body"):
            expected_body = b"<h1>%s</h1>" % str.encode(behavior)
        else:
            if suffix in {
                    "no_body_200_no_length",
                    "no_body_200_length",
            } or behavior in {
                    "request_pre_body_generates_response_without_body",
                    "request_body_last_generates_response_without_body",
            } or behavior.endswith(
                    "response_pre_body_generates_response_without_body"):
                expected_body = b""
            else:
                expected_body = b"<h1>body_200</h1>" if body_length is None else (
                    b"<h1>body_200 %d</h1>" % body_length)
            if behavior.endswith("response_pre_body_prepend"
                                ) or behavior.endswith("response_body_prepend"):
                expected_body = b"<h1>Pre body</h1>" + expected_body
            elif behavior.endswith("response_body_append"):
                expected_body += b"<h1>Post body</h1>"
        read_body = response.read()
        if behavior in {"body_200_chunked", "body_200_chunked_padded_length"}:
            remaining_body = read_body
            read_body = ""
            while True:
                carriage_return_index = remaining_body.index("\r")
                chunk_length = int(remaining_body[:carriage_return_index], 16)
                if chunk_length == 0:
                    break
                read_body += remaining_body[carriage_return_index +
                                            2:carriage_return_index + 2 +
                                            chunk_length]
                remaining_body = remaining_body[carriage_return_index + 2 +
                                                chunk_length + 2]
        assert read_body == expected_body, ("%s body %s doesn't match %s!" %
                                            (url, read_body, expected_body))

        http_connection.close()

        assert expected_request_counter == request_counter.value(), (
            "Unexpected request_count - expected %d was %d",
            expected_request_counter, request_counter.value())

        test_util.runner.get_line_from_queue_and_assert(
            queue, "connection_finished\n")

    for behavior in [
            "default",
            "buffer_request_default",
            "request_pre_body_generates_response_with_body",
            "request_pre_body_generates_response_without_body",
            "request_body_last_generates_response_with_body",
            "request_body_last_generates_response_without_body",
            "response_pre_body_generates_response_with_body",
            "response_pre_body_generates_response_without_body",
            "buffer_request_response_pre_body_generates_response_with_body",
            "buffer_request_response_pre_body_generates_response_without_body",
            "response_pre_body_prepend",
            "response_body_prepend",
            "response_body_append",
            "buffer_request_response_pre_body_prepend",
            "buffer_request_response_body_prepend",
            "buffer_request_response_body_append",
    ]:
        for send_immediately in [True, False]:
            queue, proxy_process = test_util.runner.run(
                "./tests-proxy/server/switch_callbacks_proxy",
                [behavior, "immediately" if send_immediately else "collect"])

            proxy_port = int(queue.get().strip())

            for method in ["HEAD", "GET", "POST"]:
                for suffix in [
                        "body_200_length",
                        "body_200_no_length",
                        "body_200_chunked",
                        "body_200_chunked_padded_length",
                        "no_body_200_length",
                        "no_body_200_no_length",
                ]:
                    test_requests(proxy_port, behavior, send_immediately,
                                  method, suffix)

            proxy_process.kill()
