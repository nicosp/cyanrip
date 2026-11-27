#!/usr/bin/env python3
# A mock GNUdb (CDDB over HTTP) server, for testing cyanrip's lookups.
#
# It answers GET /~cddb/cddb.cgi?cmd=...&hello=...&proto=... from the
# fixtures in tests/fixtures/gnudb, which are the bodies GNUdb returns:
#
#   query-<discid>.txt            reply to "cddb query <discid> ..."
#   read-<category>-<discid>.txt  reply to "cddb read <category> <discid>"
#
# A query or read without a fixture gets the protocol's "no match" (202) or
# "entry not found" (401) reply, and anything else a 500.  Files named
# live-*.txt are replies recorded from gnudb.gnudb.org; tests can serve them
# with respond().  Every request is kept in .requests, so tests can check
# what cyanrip sent.
#
# Standalone, for trying cyanrip by hand:
#
#   tests/gnudb_server.py [--port 8880] [--fixtures DIR]
#   cyanrip --gnudb-email test@example.invalid --gnudb-host 127.0.0.1:8880 ...

import argparse
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "gnudb"
PATH = "/~cddb/cddb.cgi"


class Request:
    def __init__(self, cmd, hello, proto, user_agent):
        self.cmd = cmd
        self.hello = hello
        self.proto = proto
        self.user_agent = user_agent

    def __repr__(self):
        return f"Request(cmd={self.cmd!r}, hello={self.hello!r}, proto={self.proto!r})"


class GnudbServer:
    def __init__(self, fixtures=FIXTURES, port=0):
        self.fixtures = Path(fixtures)
        self.requests = []
        # cmd prefix -> (HTTP status, body), set by respond()
        self.overrides = {}
        server = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                url = urlsplit(self.path)
                if url.path != PATH:
                    self.send_error(404)
                    return
                q = parse_qs(url.query, keep_blank_values=True)
                req = Request(q.get("cmd", [""])[0], q.get("hello", [""])[0],
                              q.get("proto", [""])[0],
                              self.headers.get("User-Agent", ""))
                server.requests.append(req)
                status, body = server.reply(req)
                self.send_response(status)
                self.send_header("Content-Type", "text/plain; charset=UTF-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass

        self.httpd = ThreadingHTTPServer(("127.0.0.1", port), Handler)
        self.host = f"127.0.0.1:{self.httpd.server_address[1]}"

    def fixture(self, name):
        return (self.fixtures / name).read_bytes()

    def respond(self, cmd_prefix, body, status=200):
        """Answer commands starting with cmd_prefix with body (bytes, str or
        the name of a fixture file)"""
        if isinstance(body, str) and (self.fixtures / body).is_file():
            body = self.fixture(body)
        elif isinstance(body, str):
            body = body.encode()
        self.overrides[cmd_prefix] = (status, body)

    def reply(self, req):
        for prefix, answer in self.overrides.items():
            if req.cmd.startswith(prefix):
                return answer

        # As gnudb.gnudb.org: a hello that isn't "user host app version",
        # or has an '@' in it, is a syntax error
        args = req.cmd.split()
        if len(req.hello.split()) != 4 or "@" in req.hello:
            return 200, b"500 Command syntax error.\r\n"
        if args[:2] == ["cddb", "query"] and len(args) >= 4:
            f = self.fixtures / f"query-{args[2]}.txt"
            if f.is_file():
                return 200, f.read_bytes()
            return 200, b"202 No match found\r\n"
        if args[:2] == ["cddb", "read"] and len(args) == 4:
            f = self.fixtures / f"read-{args[2]}-{args[3]}.txt"
            if f.is_file():
                return 200, f.read_bytes()
            return 200, b"401 Specified CDDB entry not found.\r\n"
        return 200, b"500 Unrecognized command.\r\n"

    def __enter__(self):
        self.thread = threading.Thread(target=self.httpd.serve_forever,
                                       daemon=True)
        self.thread.start()
        return self

    def __exit__(self, *exc):
        self.httpd.shutdown()
        self.httpd.server_close()


if __name__ == "__main__":
    p = argparse.ArgumentParser(description="Mock GNUdb server")
    p.add_argument("--port", type=int, default=8880)
    p.add_argument("--fixtures", default=FIXTURES)
    a = p.parse_args()
    with GnudbServer(a.fixtures, a.port) as s:
        print(f"Serving {a.fixtures} on {s.host}, Ctrl-C to stop")
        try:
            s.thread.join()
        except KeyboardInterrupt:
            pass
