import queue
import test_util.proxy
import test_util.runner
import ssl
import http.client

if __name__ == "__main__":
    for bump in [False, True]:
        queue, proxy_process = test_util.runner.run(
            "./tests-proxy/server/tunnel_or_bump_callbacks_proxy",
            ["bump" if bump else "tunnel"])

        proxy_port = int(queue.get().strip())

        http_connection = http.client.HTTPConnection("127.0.0.1", proxy_port)
        http_connection.connect()
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")

        for url in ["http://example.com", "http://example.com/"]:
            request = http_connection.request("GET", url)
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_pre_body /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_body_some_last /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body / 200\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")

            response = http_connection.getresponse()
            body_piece = b"<h1>Example Domain</h1>"
            assert body_piece in response.read(), ("%s has no '%s' in body!" %
                                                   (url, body_piece))

        http_connection.close()
        test_util.runner.get_line_from_queue_and_assert(
            queue, "connection_finished\n")

        if bump:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            kwargs = {"context": context}
        else:
            kwargs = {}

        https_connection = http.client.HTTPSConnection("127.0.0.1", proxy_port,
                                                       **kwargs)
        # We cannot call https_connection.connect() here as it would try
        # talking SSL instead of plain HTTP - we have to tell it about tunnel
        # first.
        https_connection.set_tunnel("example.com", 443)
        https_connection.request("GET", "/")
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")
        test_util.runner.get_line_from_queue_and_assert(
            queue, "connect example.com 443\n")

        if bump:
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_pre_body /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_body_some_last /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body / 200\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")

        response = https_connection.getresponse()
        body_piece = b"<h1>Example Domain</h1>"
        assert body_piece in response.read(), ("%s has no '%s' in body!" %
                                               (url, body_piece))

        https_connection.close()
        test_util.runner.get_line_from_queue_and_assert(
            queue, "connection_finished\n")

        # Let's test some redirects
        https_connection = http.client.HTTPSConnection("127.0.0.1", proxy_port,
                                                       **kwargs)

        # We cannot call https_connection.connect() here as it would try
        # talking SSL instead of plain HTTP - we have to tell it about tunnel
        # first.
        https_connection.set_tunnel("www.iana.org", 443)
        url = "/domains/example"
        https_connection.request(
            "GET",
            url,
            headers={
                "User-Agent":
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 11_0_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4298.0 Safari/537.36",
            })
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")
        test_util.runner.get_line_from_queue_and_assert(
            queue, "connect www.iana.org 443\n")

        if bump:
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_pre_body /domains/example\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_body_some_last /domains/example\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body /domains/example 301\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /domains/example\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")

        response = https_connection.getresponse()
        body_piece = b"<h1>Moved Permanently</h1>"
        assert body_piece in response.read(), ("%s has no '%s' in body!" %
                                               (url, body_piece))

        url = "/domains/reserved"
        https_connection.request(
            "GET",
            url,
            headers={
                "User-Agent":
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 11_0_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4298.0 Safari/537.36",
            })

        if bump:
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_pre_body /domains/reserved\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "request_body_some_last /domains/reserved\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_pre_body /domains/reserved 200\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_body_some_last /domains/reserved\n")
            test_util.runner.get_line_from_queue_and_assert(
                queue, "response_finished\n")

        response = https_connection.getresponse()
        body_piece = b"<h1>IANA-managed Reserved Domains</h1>"
        assert body_piece in response.read(), ("%s has no '%s' in body!" %
                                               (url, body_piece))

        https_connection.close()
        test_util.runner.get_line_from_queue_and_assert(
            queue, "connection_finished\n")

        proxy_process.kill()