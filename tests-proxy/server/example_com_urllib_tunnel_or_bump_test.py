import urllib.request
import test_util.proxy
import test_util.runner
import ssl

if __name__ == "__main__":
    for bump in [False, True]:
        queue, proxy_process = test_util.runner.run(
            "./tests-proxy/server/tunnel_or_bump_callbacks_proxy",
            ["bump" if bump else "tunnel"],
        )

        proxy_port = int(queue.get().strip())

        if bump:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            extra_openers = [urllib.request.HTTPSHandler(context=context)]
        else:
            extra_openers = []

        opener = test_util.proxy.get_opener(
            {
                "http": "http://127.0.0.1:%d" % proxy_port,
                "https": "http://127.0.0.1:%d" % proxy_port,
            },
            extra_openers,
        )

        for url in [
            "http://example.com",
            "http://example.com/",
            "https://example.com",
            "https://example.com/",
        ]:
            response = opener.open(url)
            assert (
                response.status == 200
            ), "%s didn't respond with status 200, it responded with %d!" % (
                url,
                response.status,
            )
            body_piece = b"<h1>Example Domain</h1>"
            assert body_piece in response.read(), "%s has no '%s' in body!" % (
                url,
                body_piece,
            )

            test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")
            https = url.startswith("https://")
            if https:
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "connect example.com 443\n"
                )
            if bump or not https:
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "request_pre_body /\n"
                )
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "request_body_some_last /\n"
                )
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "response_pre_body / 200\n"
                )
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "response_body_some_last /\n"
                )
                test_util.runner.get_line_from_queue_and_assert(
                    queue, "response_finished\n"
                )
            # urllib doesn't reuse connections
            test_util.runner.get_line_from_queue_and_assert(
                queue, "connection_finished\n"
            )

        # Let's test some 301 redirects
        https = bump
        url = "%s://www.iana.org/domains/example" % ("https" if https else "http")
        response = opener.open(url)
        body_piece = b"<h1>IANA-managed Reserved Domains</h1>"
        assert body_piece in response.read(), "%s has no '%s' in body!" % (
            url,
            body_piece,
        )

        # urllib follows redirects, doesn't understand HSTS that Chrome does so no 307
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")
        if https:
            test_util.runner.get_line_from_queue_and_assert(
                queue, "connect www.iana.org 443\n"
            )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_pre_body /domains/example\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_body_some_last /domains/example\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "response_pre_body /domains/example 301\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "response_body_some_last /domains/example\n"
        )
        test_util.runner.get_line_from_queue_and_assert(queue, "response_finished\n")
        test_util.runner.get_line_from_queue_and_assert(queue, "connection_finished\n")

        # The URL above redirected to non https
        test_util.runner.get_line_from_queue_and_assert(queue, "connection\n")
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_pre_body /domains/reserved\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "request_body_some_last /domains/reserved\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "response_pre_body /domains/reserved 200\n"
        )
        test_util.runner.get_line_from_queue_and_assert(
            queue, "response_body_some_last /domains/reserved\n"
        )
        test_util.runner.get_line_from_queue_and_assert(queue, "response_finished\n")
        test_util.runner.get_line_from_queue_and_assert(queue, "connection_finished\n")

        proxy_process.kill()
