import http.server, ssl
context = ssl.create_default_context()
server_address = ('connor-mini-pc', 4443)
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory='web', **kwargs)

httpd = http.server.HTTPServer(server_address, Handler)
httpd.socket = ssl.wrap_socket(httpd.socket,
                               server_side=True,
                               keyfile="./connor-mini-pc-key.pem",
                               certfile='./connor-mini-pc.pem',
                               ssl_version=ssl.PROTOCOL_TLS)
httpd.serve_forever()

