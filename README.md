# Proxy

Simple, extensible and fast proxy written in C++ using Boost.Asio released under MIT license. The proxy is capable of handling both HTTP as well as HTTPS traffic. HTTPS transmissions can be selectively attacked using Man-in-the-middle attack (so in other words you can decide to intercept connection to alice.example.com and just tunnel bob.example.com).

The idea is that you control what the proxy is doing using the callbacks mechanism.  The requests and responses can be intercepted, modified at every stage, including a possibility of proxy responding to requests directly without any requests going out to the upstream.

To see how to implement different flows see the tests in `proxy/tests-proxy/server/`, especially `proxy/tests-proxy/server/http_1_0_server_http_client_behaviors_test.py` and corresponding `proxy/tests-proxy/server/switch_callbacks_proxy.cpp` and `proxy/tests-proxy/server/http_1_0_server_socket_client_100_continue_test.py`.

## How to run

To run the proxy:
```
GLOG_logtostderr=1 GLOG_v=3 bazel run proxy/server:main 0.0.0.0 80
```

To run tests:
```
bazel test --test_env=GLOG_logtostderr=1 --test_env=GLOG_v=11 tests-proxy/...
```

The GLOG_* flags are optional and make the proxy log useful debugging information.


To build tags (e.g. for VSCode):
```
bazel run //compdb-proxy:compdb_copy_compile_commands
```

## Limitations

The current implementation of the proxy is single threaded, but it should be possible and relatively easy to scale it to multiple threads. Pull requests welcomed!

The current implementation has a not very cleanly implemented certificate generator. Some work is needed to clean that file. Pull requests welcomed!
