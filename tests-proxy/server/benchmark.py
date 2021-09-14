import test_util.proxy
import test_util.runner
import http.client
import http.server
import threading
import time

if __name__ == "__main__":

    # This is HTTP 1.0 server that doesn't support persisent connections
    class Server(http.server.BaseHTTPRequestHandler):

        disable_nagle_algorithm = True

        def do_GET(self):
            if self.path == "/abc/":
                self.send_response(200)
                response = b"<h1>abc</h1>"
                self.send_header("Content-type", "text/html")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)

        def log_message(self, format, *args):
            return

    server = http.server.HTTPServer(('localhost', 0), Server)
    server_port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever)
    thread.daemon = True  # thread dies with the program
    thread.start()

    queue, proxy_process = test_util.runner.run(
        "./tests-proxy/server/benchmark_proxy")

    proxy_port = int(queue.get().strip())

    for outer_index in range(5):
        iterations = 10**3
        start = time.time()
        for inner_index in range(iterations):
            http_connection = http.client.HTTPConnection(
                "127.0.0.1", proxy_port)
            http_connection.connect()

            url = "http://localhost:%d/abc/" % server_port
            http_connection.request("GET", url)
            response = http_connection.getresponse()

            http_connection.close()

        print("Time per iteration with proxy: %f seconds" %
              ((time.time() - start) / iterations))

        start = time.time()
        for inner_index in range(iterations):
            http_connection = http.client.HTTPConnection(
                "127.0.0.1", server_port)
            http_connection.connect()

            url = "/abc/"
            http_connection.request("GET", url)
            response = http_connection.getresponse()

            http_connection.close()

        print("Time per iteration without proxy: %f seconds" %
              ((time.time() - start) / iterations))

    proxy_process.kill()
