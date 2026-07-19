#!/usr/bin/env python3
"""Status dashboard + process supervisor for the Lichess bot (stdlib only).

Serves one HTML page: live state, rating tiles with trend, a rating-history
chart, outcome breakdown, and recent games - all from the cached public
Lichess API. Also supervises the lichess-bot process (start/stop buttons).
"""
import datetime
import html
import json
import os
import shlex
import signal
import subprocess
import sys
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = os.environ.get("LICHESS_BOT_TOKEN", "")
PORT = int(os.environ.get("STATUS_PORT", "8087"))
BOT_DIR = os.environ.get("BOT_DIR", "/lichess-bot")
BOT_CMD = shlex.split(os.environ.get("BOT_CMD", "python lichess-bot.py"))
UA = "chess-engine-status/2.0"

CHART_W, CHART_H = 640, 230
PAD_L, PAD_R, PAD_T, PAD_B = 44, 14, 12, 26
MAX_DAYS = 60


class Supervisor:
    """Runs lichess-bot as a child process; restarts it if it crashes."""

    def __init__(self):
        self.proc = None
        self.desired = True
        self.lock = threading.Lock()
        threading.Thread(target=self._watch, daemon=True).start()

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def start(self):
        with self.lock:
            self.desired = True
            if not self.alive():
                self.proc = subprocess.Popen(BOT_CMD, cwd=BOT_DIR)

    def stop(self):
        with self.lock:
            self.desired = False
            if self.alive():
                self.proc.send_signal(signal.SIGINT)
        if self.proc:
            try:
                self.proc.wait(timeout=20)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def _watch(self):
        while True:
            time.sleep(5)
            if self.desired and not self.alive():
                with self.lock:
                    if self.desired and not self.alive():
                        self.proc = subprocess.Popen(BOT_CMD, cwd=BOT_DIR)


supervisor = Supervisor()
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


# ---------------------------------------------------------------------------
# Rating history chart (server-rendered SVG + tiny hover script)
# ---------------------------------------------------------------------------

SERIES_SLOTS = [("Bullet", "--s1"), ("Blitz", "--s2"), ("Rapid", "--s3")]


def build_chart(u):
    hist = fetch(f"https://lichess.org/api/user/{u}/rating-history", ttl=300)
    if not hist:
        return "", ""
    by_name = {h.get("name"): h.get("points", []) for h in hist}

    # Collect per-series points as day -> rating (lichess months are 0-based)
    series = []
    all_days = set()
    for name, var in SERIES_SLOTS:
        pts = {}
        for y, m, d, r in by_name.get(name, []):
            try:
                day = datetime.date(y, m + 1, d).toordinal()
            except ValueError:
                continue
            pts[day] = r
        if pts:
            series.append({"name": name, "var": var, "pts": pts})
            all_days.update(pts)
    if not series or len(all_days) < 1:
        return "", ""

    today = datetime.date.today().toordinal()
    first = max(min(all_days), today - MAX_DAYS)
    days = list(range(first, today + 1))
    if len(days) < 2:
        days = [first - 1, first]  # degenerate but renderable

    # Forward-fill each series over the day grid (rating is flat between games)
    values_all = []
    for s in series:
        vals, cur = [], None
        for day in days:
            if day in s["pts"]:
                cur = s["pts"][day]
            vals.append(cur)
        s["vals"] = vals
        values_all += [v for v in vals if v is not None]

    lo, hi = min(values_all), max(values_all)
    span = max(hi - lo, 50)
    lo, hi = lo - span * 0.1, hi + span * 0.1

    def x(i):
        return PAD_L + (CHART_W - PAD_L - PAD_R) * i / max(len(days) - 1, 1)

    def y(v):
        return PAD_T + (CHART_H - PAD_T - PAD_B) * (1 - (v - lo) / (hi - lo))

    # Recessive grid: 4 horizontal lines at round rating values
    grid, step = "", max(25, round(span / 3 / 25) * 25)
    tick = int(lo // step + 1) * step
    while tick < hi:
        gy = round(y(tick), 1)
        grid += (f'<line x1="{PAD_L}" y1="{gy}" x2="{CHART_W - PAD_R}" y2="{gy}" class="grid"/>'
                 f'<text x="{PAD_L - 6}" y="{gy + 3.5}" class="tick" text-anchor="end">{tick}</text>')
        tick += step

    # A few x-axis date labels
    for i in range(0, len(days), max(len(days) // 4, 1)):
        d = datetime.date.fromordinal(days[i])
        grid += (f'<text x="{round(x(i), 1)}" y="{CHART_H - 8}" class="tick" '
                 f'text-anchor="middle">{d.strftime("%b %d")}</text>')

    lines, dots, labels, data_series = "", "", "", []
    for s in series:
        pts = [(round(x(i), 1), round(y(v), 1)) for i, v in enumerate(s["vals"]) if v is not None]
        if not pts:
            continue
        poly = " ".join(f"{px},{py}" for px, py in pts)
        lines += f'<polyline points="{poly}" style="stroke:var({s["var"]})" class="sline"/>'
        # direct label at the line end: ink text next to a colored mark
        ex, ey = pts[-1]
        labels += (f'<circle cx="{ex}" cy="{ey}" r="3" style="fill:var({s["var"]})"/>'
                   f'<text x="{ex - 4}" y="{ey - 7}" class="slabel" text-anchor="end">{s["name"]}</text>')
        dots += (f'<circle id="dot-{s["name"]}" r="4" style="fill:var({s["var"]})" '
                 f'class="hoverdot" visibility="hidden"/>')
        data_series.append({"name": s["name"], "vals": s["vals"], "var": s["var"]})

    chart_data = json.dumps({
        "days": [datetime.date.fromordinal(d).strftime("%b %d") for d in days],
        "xs": [round(x(i), 1) for i in range(len(days))],
        "ys": {s["name"]: [round(y(v), 1) if v is not None else None for v in s["vals"]]
               for s in series},
        "series": [{"name": s["name"], "vals": s["vals"]} for s in series],
        "padT": PAD_T, "plotB": CHART_H - PAD_B,
    })

    legend = "".join(f'<span class="chip" style="background:var({s["var"]})"></span>'
                     f'<span class="lgname">{s["name"]}</span>' for s in series)

    svg = f"""
<div class="card">
  <div class="cardhead"><span class="cardtitle">Rating</span><span class="legend">{legend}</span></div>
  <div class="chartwrap">
    <svg id="rchart" viewBox="0 0 {CHART_W} {CHART_H}" preserveAspectRatio="none">
      {grid}
      <line id="crosshair" y1="{PAD_T}" y2="{CHART_H - PAD_B}" class="cross" visibility="hidden"/>
      {lines}{labels}{dots}
      <rect id="hover" x="{PAD_L}" y="{PAD_T}" width="{CHART_W - PAD_L - PAD_R}"
            height="{CHART_H - PAD_T - PAD_B}" fill="transparent"/>
    </svg>
    <div id="tip"></div>
  </div>
</div>
<script id="chart-data" type="application/json">{chart_data}</script>"""
    return svg, chart_data


CHART_JS = """
(function () {
  const el = document.getElementById('chart-data');
  if (!el) return;
  const D = JSON.parse(el.textContent);
  const svg = document.getElementById('rchart');
  const hover = document.getElementById('hover');
  const cross = document.getElementById('crosshair');
  const tip = document.getElementById('tip');
  function nearest(px) {
    let best = 0, bd = 1e9;
    D.xs.forEach((x, i) => { const d = Math.abs(x - px); if (d < bd) { bd = d; best = i; } });
    return best;
  }
  hover.addEventListener('mousemove', (e) => {
    const r = svg.getBoundingClientRect();
    const px = (e.clientX - r.left) * (svg.viewBox.baseVal.width / r.width);
    const i = nearest(px);
    cross.setAttribute('x1', D.xs[i]); cross.setAttribute('x2', D.xs[i]);
    cross.setAttribute('visibility', 'visible');
    let rows = '<b>' + D.days[i] + '</b>';
    D.series.forEach((s) => {
      const dot = document.getElementById('dot-' + s.name);
      const y = D.ys[s.name][i];
      if (y === null) { dot.setAttribute('visibility', 'hidden'); return; }
      dot.setAttribute('cx', D.xs[i]); dot.setAttribute('cy', y);
      dot.setAttribute('visibility', 'visible');
      rows += '<div>' + s.name + ': <b>' + s.vals[i] + '</b></div>';
    });
    tip.innerHTML = rows;
    tip.style.display = 'block';
    const wrap = svg.parentElement.getBoundingClientRect();
    let lx = e.clientX - wrap.left + 14;
    if (lx > wrap.width - 120) lx = e.clientX - wrap.left - 130;
    tip.style.left = lx + 'px';
    tip.style.top = Math.max(e.clientY - wrap.top - 20, 0) + 'px';
  });
  hover.addEventListener('mouseleave', () => {
    cross.setAttribute('visibility', 'hidden');
    tip.style.display = 'none';
    D.series.forEach((s) =>
      document.getElementById('dot-' + s.name).setAttribute('visibility', 'hidden'));
  });
})();
"""

CSS = """
:root { --surface:#fcfcfb; --ink:#1f1f1e; --muted:#6f6e6a; --line:#e6e5e1;
        --good:#0ca30c; --bad:#c05a36; --accent:#2a78d6;
        --s1:#2a78d6; --s2:#1baf7a; --s3:#eda100; }
@media (prefers-color-scheme: dark) {
  :root { --surface:#1a1a19; --ink:#ececea; --muted:#989792; --line:#33322f;
          --good:#3fbf3f; --bad:#ec835a; --accent:#86b6ef;
          --s1:#3987e5; --s2:#199e70; --s3:#c98500; }
}
* { box-sizing:border-box; margin:0; }
body { background:var(--surface); color:var(--ink); max-width:680px;
       font:15px/1.5 system-ui,-apple-system,sans-serif; margin:0 auto; padding:24px 16px; }
h1 { font-size:22px; } h1 a { color:inherit; text-decoration:none; }
.sub { color:var(--muted); margin-bottom:16px; }
.sub a { color:var(--accent); }
.dot { display:inline-block; width:9px; height:9px; border-radius:50%;
       background:var(--muted); margin-right:5px; }
.dot.on { background:var(--good); }
.controls { display:flex; gap:8px; margin-bottom:18px; }
.controls form { display:inline; }
button { font:14px system-ui; padding:6px 14px; border-radius:6px; cursor:pointer;
         border:1px solid var(--line); background:transparent; color:var(--ink); }
button.primary { background:var(--accent); border-color:var(--accent); color:#fff; }
.chipstate { font-size:13px; color:var(--muted); align-self:center; margin-left:4px; }
.tiles { display:grid; grid-template-columns:repeat(auto-fit,minmax(120px,1fr));
         gap:10px; margin-bottom:14px; }
.tile { border:1px solid var(--line); border-radius:8px; padding:10px 12px; }
.tile .k { color:var(--muted); font-size:12px; text-transform:uppercase; letter-spacing:.04em; }
.tile .v { font-size:24px; font-weight:600; margin-top:2px; }
.tile .v small { font-size:13px; color:var(--muted); font-weight:400; }
.up { color:var(--good); font-size:13px; } .down { color:var(--bad); font-size:13px; }
.card { border:1px solid var(--line); border-radius:8px; padding:12px; margin-bottom:14px; }
.cardhead { display:flex; justify-content:space-between; margin-bottom:6px; }
.cardtitle { color:var(--muted); font-size:12px; text-transform:uppercase; letter-spacing:.04em; }
.legend { display:flex; gap:6px; align-items:center; }
.chip { width:10px; height:10px; border-radius:3px; display:inline-block; }
.lgname { font-size:12px; color:var(--muted); margin-right:6px; }
.chartwrap { position:relative; }
#rchart { width:100%; height:auto; display:block; }
.grid { stroke:var(--line); stroke-width:1; }
.tick { fill:var(--muted); font-size:10px; }
.slabel { fill:var(--ink); font-size:11px; }
.sline { fill:none; stroke-width:2; stroke-linejoin:round; }
.cross { stroke:var(--muted); stroke-width:1; stroke-dasharray:3 3; }
.hoverdot { stroke:var(--surface); stroke-width:2; }
#tip { position:absolute; display:none; pointer-events:none; background:var(--surface);
       border:1px solid var(--line); border-radius:6px; padding:6px 9px; font-size:12px;
       box-shadow:0 2px 8px rgba(0,0,0,.12); min-width:100px; }
.bar { display:flex; gap:2px; height:14px; border-radius:4px; overflow:hidden; margin:8px 0 6px; }
.bar span { display:block; }
.bar .w { background:var(--good); } .bar .d { background:var(--muted); } .bar .l { background:var(--bad); }
.barlabels { font-size:13px; color:var(--muted); }
.barlabels b { color:var(--ink); }
table { width:100%; border-collapse:collapse; }
th { text-align:left; color:var(--muted); font-size:12px; text-transform:uppercase;
     letter-spacing:.04em; font-weight:500; padding:4px 8px; }
td { padding:6px 8px; border-top:1px solid var(--line); }
td a { color:var(--accent); text-decoration:none; }
.win { color:var(--good); font-weight:600; } .loss { color:var(--bad); font-weight:600; }
.mut { color:var(--muted); font-weight:400; font-size:13px; }
.foot { color:var(--muted); font-size:12px; margin-top:18px; }
"""


def controls():
    running = supervisor.alive()
    if running:
        buttons = ('<form method="post" action="/stop"><button>stop bot</button></form>'
                   '<form method="post" action="/restart"><button>restart bot</button></form>')
    else:
        buttons = '<form method="post" action="/start"><button class="primary">start bot</button></form>'
    chip = "bot process: running" if running else "bot process: stopped"
    return f'<div class="controls">{buttons}<span class="chipstate">{chip}</span></div>'


def page():
    u = username()
    if not u:
        return ("<h1>chess-engine bot</h1><p>Waiting for Lichess connection "
                "(no username yet - is LICHESS_BOT_TOKEN set?)</p>" + controls())

    esc = html.escape
    user = fetch(f"https://lichess.org/api/user/{u}", ttl=60) or {}
    status = fetch(f"https://lichess.org/api/users/status?ids={u}&withGameIds=true", ttl=15)
    st = status[0] if status else {}
    games = fetch(f"https://lichess.org/api/games/user/{u}?max=12",
                  {"Accept": "application/x-ndjson"}, ttl=60, ndjson=True) or []

    playing = st.get("playing")
    state = ("playing" if playing else "online") if st.get("online") else "offline"
    dot = "dot on" if st.get("online") else "dot"
    now_playing = (f' - <a href="https://lichess.org/{esc(str(playing))}">watch live</a>'
                   if playing else "")

    # Rating tiles with recent trend
    perfs = user.get("perfs", {})
    tiles = ""
    for key, label in [("bullet", "Bullet"), ("blitz", "Blitz"),
                       ("rapid", "Rapid"), ("classical", "Classical")]:
        p = perfs.get(key, {})
        if not p.get("games"):
            continue
        prov = "<small>?</small>" if p.get("prov") else ""
        prog = p.get("prog", 0)
        trend = (f'<span class="up">&#9650;{prog}</span>' if prog > 0 else
                 f'<span class="down">&#9660;{-prog}</span>' if prog < 0 else "")
        tiles += (f'<div class="tile"><div class="k">{label}</div>'
                  f'<div class="v">{p.get("rating", "-")}{prov} {trend}</div>'
                  f'<small class="mut">{p.get("games", 0)} games</small></div>')

    c = user.get("count", {})
    tiles += (f'<div class="tile"><div class="k">Games</div>'
              f'<div class="v">{c.get("all", 0)}</div>'
              f'<small class="mut">{c.get("rated", 0)} rated</small></div>')

    chart_svg, _ = build_chart(u)

    # Outcome breakdown bar (labels carry the meaning; color is reinforcement)
    w, d, l = c.get("win", 0), c.get("draw", 0), c.get("loss", 0)
    total = max(w + d + l, 1)
    outcome = f"""
<div class="card"><div class="cardhead"><span class="cardtitle">Outcomes</span></div>
<div class="bar">
  <span class="w" style="width:{100 * w / total:.1f}%"></span>
  <span class="d" style="width:{100 * d / total:.1f}%"></span>
  <span class="l" style="width:{100 * l / total:.1f}%"></span>
</div>
<div class="barlabels"><b>{w}</b> wins &middot; <b>{d}</b> draws &middot; <b>{l}</b> losses
&middot; {100 * w / total:.0f}% won</div></div>"""

    def when(ms):
        if not ms:
            return ""
        dt = datetime.datetime.fromtimestamp(ms / 1000)
        mins = (time.time() - ms / 1000) / 60
        if mins < 60:
            return f"{int(mins)}m ago"
        if mins < 60 * 24:
            return f"{int(mins // 60)}h ago"
        return dt.strftime("%b %d")

    rows = ""
    for g in games:
        players = g.get("players", {})
        white, black = players.get("white", {}), players.get("black", {})
        we_are_white = white.get("user", {}).get("name", "").lower() == u.lower()
        us, them = (white, black) if we_are_white else (black, white)
        opp = them.get("user", {}).get("name", "?")
        opp_rating = them.get("rating")
        opp_txt = f'{esc(opp)} <span class="mut">({opp_rating})</span>' if opp_rating else esc(opp)
        winner = g.get("winner")
        if not winner:
            res, cls = "draw", ""
        elif (winner == "white") == we_are_white:
            res, cls = "win", "win"
        else:
            res, cls = "loss", "loss"
        diff = us.get("ratingDiff")
        if diff is not None:
            res += f' <span class="mut">{"+" if diff >= 0 else ""}{diff}</span>'
        gid = g.get("id", "")
        rows += (f'<tr><td><a href="https://lichess.org/{esc(gid)}">'
                 f'{esc(g.get("speed", "?"))}</a></td>'
                 f'<td>{opp_txt}</td>'
                 f'<td>{"white" if we_are_white else "black"}</td>'
                 f'<td class="{cls}">{res}</td>'
                 f'<td class="mut">{when(g.get("lastMoveAt") or g.get("createdAt"))}</td></tr>')

    games_table = f"""
<div class="card"><div class="cardhead"><span class="cardtitle">Recent games</span></div>
<table><tr><th>Game</th><th>Opponent</th><th>Color</th><th>Result</th><th>When</th></tr>{rows}</table></div>"""

    return f"""<h1><a href="https://lichess.org/@/{esc(u)}">{esc(u)}</a></h1>
<div class="sub"><span class="{dot}"></span>{state}{now_playing}</div>
{controls()}
<div class="tiles">{tiles}</div>
{chart_svg}
{outcome}
{games_table}
<div class="foot">Auto-refreshes every 60s - data from the public Lichess API -
to update the engine run ./bot.sh update-app on the server</div>"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = f"""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="60"><title>chess-engine bot</title>
<style>{CSS}</style></head><body>{page()}
<script>{CHART_JS}</script></body></html>"""
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path == "/start":
            supervisor.start()
        elif self.path == "/stop":
            supervisor.stop()
        elif self.path == "/restart":
            supervisor.stop()
            supervisor.start()
        self.send_response(303)
        self.send_header("Location", "/")
        self.end_headers()

    def log_message(self, *args):  # keep the container log clean
        pass


def shutdown(*_):
    supervisor.stop()
    sys.exit(0)


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, shutdown)  # docker stop: quit bot cleanly
    signal.signal(signal.SIGINT, shutdown)
    supervisor.start()
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
