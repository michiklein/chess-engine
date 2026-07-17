#!/usr/bin/env python3
"""Tiny status page for the Lichess bot (stdlib only).

Serves one HTML page with the bot's online state, ratings, record, and
recent games, fetched from the public Lichess API (cached to stay well
under rate limits). Gives the ZimaOS/CasaOS app tile a real web UI.
"""
import html
import json
import os
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = os.environ.get("LICHESS_BOT_TOKEN", "")
PORT = int(os.environ.get("STATUS_PORT", "8087"))
UA = "chess-engine-status/1.0"

_cache: dict = {}

def fetch(url, headers=None, ttl=60, ndjson=False):
    now = time.time()
    hit = _cache.get(url)
    if hit and hit[0] > now:
        return hit[1]
    req = urllib.request.Request(url, headers={"User-Agent": UA, **(headers or {})})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            raw = r.read().decode()
        data = ([json.loads(l) for l in raw.splitlines() if l.strip()]
                if ndjson else json.loads(raw))
        _cache[url] = (now + ttl, data)
        return data
    except Exception:
        return hit[1] if hit else None  # serve stale data over nothing

_username = None

def username():
    global _username
    if _username is None and TOKEN:
        acct = fetch("https://lichess.org/api/account",
                     {"Authorization": f"Bearer {TOKEN}"}, ttl=3600)
        if acct:
            _username = acct.get("username")
    return _username

CSS = """
:root { --surface:#fcfcfb; --ink:#1f1f1e; --muted:#6f6e6a; --line:#e6e5e1;
        --good:#0ca30c; --bad:#c05a36; --accent:#2f6fce; }
@media (prefers-color-scheme: dark) {
  :root { --surface:#1a1a19; --ink:#ececea; --muted:#989792; --line:#33322f;
          --good:#3fbf3f; --bad:#ec835a; --accent:#86b6ef; }
}
* { box-sizing:border-box; margin:0; }
body { background:var(--surface); color:var(--ink); max-width:640px;
       font:15px/1.5 system-ui,-apple-system,sans-serif; margin:0 auto; padding:24px 16px; }
h1 { font-size:22px; } h1 a { color:inherit; text-decoration:none; }
.sub { color:var(--muted); margin-bottom:20px; }
.dot { display:inline-block; width:9px; height:9px; border-radius:50%;
       background:var(--muted); margin-right:5px; }
.dot.on { background:var(--good); }
.tiles { display:grid; grid-template-columns:repeat(auto-fit,minmax(110px,1fr));
         gap:10px; margin-bottom:22px; }
.tile { border:1px solid var(--line); border-radius:8px; padding:10px 12px; }
.tile .k { color:var(--muted); font-size:12px; text-transform:uppercase; letter-spacing:.04em; }
.tile .v { font-size:24px; font-weight:600; margin-top:2px; }
.tile .v small { font-size:13px; color:var(--muted); font-weight:400; }
table { width:100%; border-collapse:collapse; }
th { text-align:left; color:var(--muted); font-size:12px; text-transform:uppercase;
     letter-spacing:.04em; font-weight:500; padding:4px 8px; }
td { padding:6px 8px; border-top:1px solid var(--line); }
td a { color:var(--accent); text-decoration:none; }
.win { color:var(--good); font-weight:600; } .loss { color:var(--bad); font-weight:600; }
.foot { color:var(--muted); font-size:12px; margin-top:20px; }
"""

def page():
    u = username()
    if not u:
        return "<h1>chess-engine bot</h1><p>Waiting for Lichess connection " \
               "(no username yet - is LICHESS_BOT_TOKEN set?)</p>"

    esc = html.escape
    user = fetch(f"https://lichess.org/api/user/{u}", ttl=60) or {}
    status = fetch(f"https://lichess.org/api/users/status?ids={u}&withGameIds=true", ttl=15)
    st = status[0] if status else {}
    games = fetch(f"https://lichess.org/api/games/user/{u}?max=10",
                  {"Accept": "application/x-ndjson"}, ttl=60, ndjson=True) or []

    playing = st.get("playing")
    state = ("playing" if playing else "online") if st.get("online") else "offline"
    dot = "dot on" if st.get("online") else "dot"

    perfs = user.get("perfs", {})
    tiles = ""
    for key, label in [("bullet", "Bullet"), ("blitz", "Blitz"),
                       ("rapid", "Rapid"), ("classical", "Classical")]:
        p = perfs.get(key, {})
        prov = "<small>?</small>" if p.get("prov") else ""
        tiles += (f'<div class="tile"><div class="k">{label}</div>'
                  f'<div class="v">{p.get("rating", "-")}{prov}'
                  f' <small>{p.get("games", 0)} games</small></div></div>')

    c = user.get("count", {})
    tiles += (f'<div class="tile"><div class="k">Record</div>'
              f'<div class="v">{c.get("win", 0)}-{c.get("loss", 0)}-{c.get("draw", 0)}'
              f' <small>W-L-D</small></div></div>')

    rows = ""
    for g in games:
        white = g.get("players", {}).get("white", {}).get("user", {}).get("name", "?")
        black = g.get("players", {}).get("black", {}).get("user", {}).get("name", "?")
        we_are_white = white.lower() == u.lower()
        opp = black if we_are_white else white
        winner = g.get("winner")
        if not winner:
            res, cls = "draw", ""
        elif (winner == "white") == we_are_white:
            res, cls = "win", "win"
        else:
            res, cls = "loss", "loss"
        gid = g.get("id", "")
        rows += (f'<tr><td><a href="https://lichess.org/{esc(gid)}">'
                 f'{esc(g.get("speed", "?"))}</a></td>'
                 f'<td>{esc(opp)}</td>'
                 f'<td>{"white" if we_are_white else "black"}</td>'
                 f'<td class="{cls}">{res}</td></tr>')

    now_playing = (f' - <a href="https://lichess.org/{esc(str(playing))}">watch live</a>'
                   if playing else "")

    return f"""<h1><a href="https://lichess.org/@/{esc(u)}">{esc(u)}</a></h1>
<div class="sub"><span class="{dot}"></span>{state}{now_playing}</div>
<div class="tiles">{tiles}</div>
<table><tr><th>Game</th><th>Opponent</th><th>Color</th><th>Result</th></tr>{rows}</table>
<div class="foot">Auto-refreshes every 30s - data from the public Lichess API</div>"""

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = f"""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="30"><title>chess-engine bot</title>
<style>{CSS}</style></head><body>{page()}</body></html>"""
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *args):  # keep the container log clean
        pass

if __name__ == "__main__":
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
