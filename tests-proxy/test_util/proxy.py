import urllib.request


def get_opener(proxy_conf, extra_openers=[]):
    proxy_support = urllib.request.ProxyHandler(proxy_conf)
    return urllib.request.build_opener(*([proxy_support] + extra_openers))
