# Proxy

Simple, extensible and fast proxy written in C++ using Boost.Asio released under MIT license. The proxy is capable of handling both HTTP as well as HTTPS traffic. HTTPS transmissions can be selectively attacked using Man-in-the-middle attack (so in other words you can decide to intercept connection to alice.example.com and just tunnel bob.example.com).

The idea is that you control what the proxy is doing using the callbacks mechanism.  The requests and responses can be intercepted, modified at every stage, including a possibility of proxy responding to requests directly without any requests going out to the upstream.

To see how to implement different flows see the tests in `proxy/tests-proxy/server/`, especially `proxy/tests-proxy/server/http_1_0_server_http_client_behaviors_test.py` and corresponding `proxy/tests-proxy/server/switch_callbacks_proxy.cpp` and `proxy/tests-proxy/server/http_1_0_server_socket_client_100_continue_test.py`.

## How to run

To run the proxy:
```
GLOG_logtostderr=1 GLOG_v=3 bazel run --config debug proxy/server:main 0.0.0.0 80
```

To run tests:
```
bazel test --config debug --test_env=GLOG_logtostderr=1 --test_env=GLOG_v=11 tests-proxy/...
```

The `GLOG_*` flags are optional and make the proxy log useful debugging information (only for debug builds).
`--config debug` is optional too.


To build tags (e.g. for VSCode):
```
bazel run --config debug //compdb-proxy:compdb_copy_compile_commands
```

[MacOS specific] Certificate can be installed in the Keychain using command like:
```
sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain bazel-bin/proxy/server/main.runfiles/__main__/ca.pem
```

You can use the proxy by pointing your browser to use it, e.g. for Chromium:

```
Chromium --proxy-server=localhost:80
```

or [MacOS specific] it can be added system-wide using:

```
networksetup -setsecurewebproxy wi-fi localhost 80
networksetup -setwebproxy wi-fi localhost 80
```

To undo the last steps execute:

```
networksetup -setsecurewebproxystate wi-fi off
networksetup -setwebproxystate wi-fi off
```

## Limitations

* The current implementation of the proxy is single threaded, but it should be possible and relatively easy to scale it to multiple threads. Pull requests welcomed!
* The current implementation has a not very cleanly implemented certificate generator. Some work is needed to clean that file. Pull requests welcomed!
* Tests are engineered to run in parallel (when they start a proxy they start it on a first free port, the same when they have to run a HTTP server), but they are generate ca.key and ca.pem files in a per test folder which could in theory cause problems if the same test is run simultaneously multiple times and one test reading a half written certificate produced by another simultaneously running instance of the test (we are yet to see this in the wild and we have ran the tests in parallel multiple times). You have been warned.
* Current SSL certificate verification doesn't reject certificates for [https://pinning-test.badssl.com/](https://pinning-test.badssl.com/) and [https://revoked.badssl.com/](https://revoked.badssl.com/).

