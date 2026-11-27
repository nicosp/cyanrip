#!/usr/bin/env python3
# Records gnudb.gnudb.org's replies to CDDB commands, as fixtures for the
# mock server (gnudb_server.py), which the "gnudb" test replays.
#
#   GNUDB_EMAIL=you@example.com tests/gnudb_record.py NAME 'CDDB COMMAND' ...
#
# Each reply is saved as tests/fixtures/gnudb/live-NAME.txt.  The request is
# the one cyanrip makes: the hello is "<GNUDB_EMAIL> localhost cyanrip
# <version>", with proto 6.  The address is replaced with EMAIL in what's
# saved.  For example:
#
#   GNUDB_EMAIL=... tests/gnudb_record.py \
#       query-unknown-disc 'cddb query 00000001 1 150 100' \
#       read-unknown 'cddb read rock 00000001' \
#       bad-command 'cddb frobnicate'

import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

HOST = os.environ.get("GNUDB_HOST", "gnudb.gnudb.org")
VERSION = os.environ.get("CYANRIP_VERSION", "0.9.4-rc3")
FIXTURES = Path(__file__).resolve().parent / "fixtures" / "gnudb"

email = os.environ.get("GNUDB_EMAIL")
args = sys.argv[1:]
if not email or not args or len(args) % 2:
    sys.exit("usage: GNUDB_EMAIL=... gnudb_record.py NAME 'CDDB COMMAND' ...")

for i in range(0, len(args), 2):
    name, cmd = args[i], args[i + 1]
    query = urllib.parse.urlencode({
        "cmd": cmd,
        "hello": f"{email} localhost cyanrip {VERSION}",
        "proto": "6",
    })
    req = urllib.request.Request(
        f"http://{HOST}/~cddb/cddb.cgi?{query}",
        headers={"User-Agent": f"cyanrip/{VERSION} ( https://github.com/cyanreg/cyanrip )"})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            status, body = r.status, r.read()
    except urllib.error.HTTPError as e:
        status, body = e.code, e.read()

    body = body.replace(email.encode(), b"EMAIL")
    out = FIXTURES / f"live-{name}.txt"
    out.write_bytes(body)
    first = body.split(b"\n", 1)[0].decode(errors="replace").strip()
    print(f"{out.name}: HTTP {status}: {first}")
    if status != 200:
        print(f"  not a CDDB reply; delete {out.name} unless that's the point")
    time.sleep(1)
