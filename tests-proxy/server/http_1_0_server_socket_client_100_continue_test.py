import test_util.proxy
import test_util.runner
import http
import http.server
import threading
import test_util.thread_safe_counter
import random
import socket
import time

if __name__ == "__main__":

    request_counter = test_util.thread_safe_counter.Counter()

    # This is HTTP 1.1 server that does not support persisent connections
    class Server(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        disable_nagle_algorithm = True

        def handle_expect_100(self):
            if self.path == "/reject_continue_no_body/":
                self.send_response(404)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return False
            elif self.path == "/reject_continue_body/":
                self.send_response(404)
                self.send_header("Content-Length", "4")
                self.end_headers()
                self.wfile.flush()
                time.sleep(0.01)
                self.wfile.write(b"nope")
                return False
            else:
                self.send_response_only(http.HTTPStatus.CONTINUE)
                self.end_headers()
                return True

        def do_POST(self):
            self.handle_expect_100
            request_counter.increment()
            content_length = int(self.headers['Content-Length'])
            body = self.rfile.read(content_length) if content_length > 0 else ""
            body_length = len(body)
            if self.path == "/body_200_length/":
                self.send_response(200)
                response = b"<h1>body_200 %d</h1>" % body_length
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
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

    def test_requests(proxy_port, behavior, send_immediately, suffix):
        if suffix.startswith("reject_continue_") and behavior != "default":
            return True
        global expected_request_counter
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(("127.0.0.1", proxy_port))
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")

        url = "http://localhost:%d/%s/" % (server_port, suffix)
        # We want to often test short payloads that will fit in the same
        # packet with the pre body.
        body_length = random.choice(
            [random.randint(1, 2),
             random.randint(3, 100000)])
        body = b"a" * body_length
        client.send(b"POST %s HTTP/1.1\r\n"
                    b"Host: localhost:%d\r\n"
                    b"Accept-Encoding: identity\r\n"
                    b"Content-Length: %d\r\n"
                    b"Expect: 100-continue\r\n\r\n" %
                    (url.encode(), proxy_port, len(body)))

        stream = "downstream" if behavior in {
            "request_pre_body_generates_response_with_body",
            "request_pre_body_generates_response_without_body",
            "request_body_last_generates_response_with_body",
            "request_body_last_generates_response_without_body",
            "request_pre_body_generates_404_response_with_body",
            "request_pre_body_generates_404_response_without_body",
        } else "upstream"
        if not suffix.startswith("reject_continue_") and (
                behavior.endswith("default") or behavior.endswith(
                    "response_pre_body_generates_response_with_body") or
                behavior.endswith(
                    "response_pre_body_generates_response_without_body") or
                behavior.endswith("response_pre_body_prepend") or
                behavior.endswith("response_body_prepend") or
                behavior.endswith("response_body_append")):
            expected_request_counter += 1
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_pre_body /%s/ %s\n" % (suffix, stream))

        if behavior not in {
                "request_pre_body_generates_response_with_body",
                "request_pre_body_generates_response_without_body",
                "request_body_last_generates_response_with_body",
                "request_body_last_generates_response_without_body",
                "request_pre_body_generates_404_response_with_body",
                "request_pre_body_generates_404_response_without_body",
        }:
            code = 200 if behavior in {
                "response_pre_body_generates_response_with_body",
                "response_pre_body_generates_response_without_body"
            } else (404 if suffix.startswith("reject_continue") else 100)
            test_util.runner.get_line_from_queue_and_assert(
                queue,
                "response_pre_body /%s/ %d downstream\n" % (suffix, code))

        response = b""
        while response.count(b"\n") < 2:
            response += client.recv(4096)
        if suffix.startswith("reject_continue_") or behavior.startswith(
                "request_pre_body_generates_404_response_"):
            assert response.startswith(
                b"HTTP/1.1 404 Not Found\r\n"), "Didn't get 404"

            if suffix.startswith("reject_continue_"):
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "response_body_some_last /%s/\n" % suffix)
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")
        else:
            expected_response = b"HTTP/1.1 100 Continue\r\n\r\n"

            assert response.startswith(
                expected_response), "Didn't get 100 Continue"
            response = response[len(expected_response):]

            client.send(body)

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
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")

            if behavior in {
                    "request_pre_body_generates_response_with_body",
                    "request_body_last_generates_response_with_body",
            } or behavior.endswith(
                    "response_pre_body_generates_response_with_body"):
                expected_body = b"<h1>%s</h1>" % str.encode(behavior)
            else:
                if suffix == "no_body_200_length" or behavior in {
                        "request_pre_body_generates_response_without_body",
                        "request_body_last_generates_response_without_body",
                } or behavior.endswith(
                        "response_pre_body_generates_response_without_body"):
                    expected_body = b""
                else:
                    expected_body = b"<h1>body_200</h1>" if body_length is None else (
                        b"<h1>body_200 %d</h1>" % body_length)
                if behavior.endswith(
                        "response_pre_body_prepend") or behavior.endswith(
                            "response_body_prepend"):
                    expected_body = b"<h1>Pre body</h1>" + expected_body
                elif behavior.endswith("response_body_append"):
                    expected_body += b"<h1>Post body</h1>"

            expected_length = 10**10
            first = True
            while len(response) < expected_length:
                if not first:
                    piece = client.recv(4096)
                    if len(piece) == 0:
                        break
                    response += piece
                first = False
                body_end = response.find(b"\r\n\r\n")
                if body_end > -1:
                    headers = response[:body_end].split(b"\r\n")[1:]
                    for header in headers:
                        if header.startswith(b"Content-Length: "):
                            body_len = int(header[len(b"Content-Length: "):])
                            expected_length = body_end + 4 + body_len
                            break
                    else:
                        assert False, "Invalid response"
            read_body = response[response.index(b"\r\n\r\n") + 4:]
            assert read_body == expected_body, ("%s body %s doesn't match %s!" %
                                                (url, read_body, expected_body))

        client.close()

        assert expected_request_counter == request_counter.value(), (
            "Unexpected request_count - expected %d was %d",
            expected_request_counter, request_counter.value())

        test_util.runner.get_line_from_queue_and_assert(
            queue, "connection_finished\n")

    for behavior in [
            "default",
            "request_pre_body_generates_response_with_body",
            "request_pre_body_generates_response_without_body",
            "request_body_last_generates_response_with_body",
            "request_body_last_generates_response_without_body",
            "response_pre_body_generates_response_with_body",
            "response_pre_body_generates_response_without_body",
            "response_pre_body_prepend",
            "response_body_prepend",
            "response_body_append",
            "request_pre_body_generates_404_response_with_body",
            "request_pre_body_generates_404_response_without_body",
    ]:
        for send_immediately in [True, False]:
            queue, proxy_process = test_util.runner.run(
                "./tests-proxy/server/switch_callbacks_proxy",
                [behavior, "immediately" if send_immediately else "collect"])

            proxy_port = int(queue.get().strip())

            for suffix in [
                    "body_200_length",
                    "no_body_200_length",
                    "reject_continue_body",
                    "reject_continue_no_body",
            ]:
                test_requests(proxy_port, behavior, send_immediately, suffix)

            proxy_process.kill()
