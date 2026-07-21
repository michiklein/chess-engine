#!/usr/bin/env python3
"""Status dashboard + process supervisor for the Lichess bot (stdlib only).

One HTML page: live state, rating tiles + history chart, performance
breakdowns (by color, speed, opening), recent games - all from the cached
public Lichess API. Supervises the lichess-bot process (start/stop) and can
hot-swap the engine binary from the latest GitHub release (update button).
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
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = os.environ.get("LICHESS_BOT_TOKEN", "")
PORT = int(os.environ.get("STATUS_PORT", "8087"))
BOT_DIR = os.environ.get("BOT_DIR", "/lichess-bot")
BOT_CMD = shlex.split(os.environ.get("BOT_CMD", "python lichess-bot.py"))
ENGINE = os.environ.get("ENGINE_PATH", "/engine/chess_engine")
ENGINE_URL = os.environ.get(
    "ENGINE_URL",
    "https://github.com/michiklein/chess-engine/releases/download/engine-latest/chess_engine")
UA = "chess-engine-status/3.0"

CHART_W, CHART_H = 640, 230
PAD_L, PAD_R, PAD_T, PAD_B = 44, 14, 12, 26
MAX_DAYS = 60
NGAMES = 60  # recent games pulled for the table and by-color/speed stats
OPENINGS_FILE = os.environ.get("OPENINGS_CACHE", "/tmp/openings.json")
OPENINGS_REFRESH = 600  # seconds between incremental all-time openings pulls
PEAKS_FILE = os.environ.get("PEAKS_CACHE", "/tmp/peaks.json")


# ---------------------------------------------------------------------------
# Process supervision + engine hot-swap
# ---------------------------------------------------------------------------

class Supervisor:
    """Runs lichess-bot as a child process; restarts it if it crashes."""

    def __init__(self):
        self.proc = None
        self.desired = True
        self.lock = threading.Lock()
        self.status = ""  # transient message for the UI
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


def engine_version():
    """Ask the engine binary for its 'id name' line (contains the build id)."""
    try:
        p = subprocess.run([ENGINE], input="uci\nquit\n", capture_output=True,
                           text=True, timeout=5)
        for line in p.stdout.splitlines():
            if line.startswith("id name"):
                return line[len("id name"):].strip()
    except Exception:
        pass
    return "unknown"


def update_engine():
    """Download the latest release binary, validate it, and swap it in."""
    was_running = supervisor.alive()
    tmp = ENGINE + ".new"
    try:
        req = urllib.request.Request(ENGINE_URL, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=30) as r, open(tmp, "wb") as f:
            f.write(r.read())
        os.chmod(tmp, 0o755)
        # Validate: it must speak UCI before we trust it
        check = subprocess.run([tmp], input="uci\nquit\n", capture_output=True,
                               text=True, timeout=5)
        if "uciok" not in check.stdout:
            raise RuntimeError("downloaded binary did not respond to uci")
        supervisor.stop()
        os.replace(tmp, ENGINE)
        supervisor.status = "engine updated to " + engine_version()
    except Exception as e:
        supervisor.status = "update failed: " + str(e)
        try:
            os.remove(tmp)
        except OSError:
            pass
    finally:
        if was_running:
            supervisor.start()


# ---------------------------------------------------------------------------
# Lichess API (cached)
# ---------------------------------------------------------------------------

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


# Local, resettable rating peaks: start at the current rating and ratchet
# up; the reset button sets them back to the current rating.
_peaks_lock = threading.Lock()


def load_peaks():
    try:
        with open(PEAKS_FILE) as f:
            return json.load(f)
    except Exception:
        return {}


def save_peaks(p):
    try:
        tmp = PEAKS_FILE + ".tmp"
        with open(tmp, "w") as f:
            json.dump(p, f)
        os.replace(tmp, PEAKS_FILE)
    except Exception:
        pass


def track_peaks(current):
    """current: {speed: rating}; returns updated peaks dict."""
    with _peaks_lock:
        p = load_peaks()
        changed = False
        for k, r in current.items():
            if r is not None and r > p.get(k, 0):
                p[k] = r
                changed = True
        if changed:
            save_peaks(p)
        return p


def reset_peaks():
    u = username()
    if not u:
        return
    user = fetch(f"https://lichess.org/api/user/{u}", ttl=0) or {}
    perfs = user.get("perfs", {})
    with _peaks_lock:
        save_peaks({k: p.get("rating") for k, p in perfs.items()
                    if p.get("games") and p.get("rating")})


def opening_family(name):
    return (name or "Unknown").split(":")[0].split(",")[0].strip() or "Unknown"


# All-time openings, accumulated incrementally and persisted so we never
# re-download the whole game history on each page load.
_openings_last = 0.0
_openings_lock = threading.Lock()


STORE_VERSION = 4  # bump when the accumulated schema changes -> forces a rebuild


def _new_sub():
    # one tally group, sliceable independently for all / bots / humans
    return {"white": {}, "black": {}, "speed": {}, "buckets": {}, "opps": {},
            "best_win": None, "worst_loss": None}


def _new_store():
    return {"v": STORE_VERSION, "since": 0,
            "all": _new_sub(), "bot": _new_sub(), "human": _new_sub()}


def _load_openings():
    try:
        with open(OPENINGS_FILE) as f:
            s = json.load(f)
        if s.get("v") != STORE_VERSION:  # old schema: rebuild from scratch
            return _new_store()
        for grp in ("all", "bot", "human"):
            s.setdefault(grp, _new_sub())
        return s
    except Exception:
        return _new_store()


def rating_bucket(r):
    if r is None:
        return None
    if r < 1800:
        return "<1800"
    if r < 2000:
        return "1800-1999"
    if r < 2200:
        return "2000-2199"
    return "2200+"


BUCKET_ORDER = ["<1800", "1800-1999", "2000-2199", "2200+"]


def _bump(d, key, res):
    rec = d.setdefault(key, {"win": 0, "draw": 0, "loss": 0, "n": 0})
    rec[res] += 1
    rec["n"] += 1


def _save_openings(store):
    try:
        tmp = OPENINGS_FILE + ".tmp"
        with open(tmp, "w") as f:
            json.dump(store, f)
        os.replace(tmp, OPENINGS_FILE)
    except Exception:
        pass


def openings_store(u):
    """Return the all-time by-color openings tally, refreshing incrementally."""
    global _openings_last
    with _openings_lock:
        store = _load_openings()
        if time.time() - _openings_last < OPENINGS_REFRESH and store["since"]:
            return store
        _openings_last = time.time()
        since = store.get("since", 0)
        url = (f"https://lichess.org/api/games/user/{u}"
               f"?opening=true&sort=dateAsc")
        if since:
            url += f"&since={since}"
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": UA, "Accept": "application/x-ndjson"})
            with urllib.request.urlopen(req, timeout=30) as r:
                for raw in r:
                    raw = raw.decode().strip()
                    if not raw:
                        continue
                    g = json.loads(raw)
                    gv = game_view(g, u)
                    side = "white" if gv["we_white"] else "black"
                    fam = opening_family(gv["opening"])
                    bucket = rating_bucket(gv["opp_rating"])
                    who = "bot" if gv["opp_title"] == "BOT" else "human"
                    for grp in ("all", who):  # accumulate into combined + its group
                        sub = store[grp]
                        _bump(sub[side], fam, gv["res"])
                        _bump(sub["speed"], gv["speed"], gv["res"])
                        _bump(sub["opps"], gv["opp"], gv["res"])
                        if bucket:
                            _bump(sub["buckets"], bucket, gv["res"])
                        r = gv["opp_rating"]
                        if r:
                            if gv["res"] == "win" and \
                                    (not sub["best_win"] or r > sub["best_win"]["rating"]):
                                sub["best_win"] = {"rating": r, "opp": gv["opp"], "id": gv["id"]}
                            if gv["res"] == "loss" and \
                                    (not sub["worst_loss"] or r < sub["worst_loss"]["rating"]):
                                sub["worst_loss"] = {"rating": r, "opp": gv["opp"], "id": gv["id"]}
                    created = g.get("createdAt", 0)
                    if created + 1 > store["since"]:
                        store["since"] = created + 1
            _save_openings(store)
        except Exception:
            pass  # keep whatever we already have
        return store


def game_view(g, u):
    """Normalize one game to our perspective: (result, we_white, opp, opp_rating)."""
    players = g.get("players", {})
    white, black = players.get("white", {}), players.get("black", {})
    we_white = white.get("user", {}).get("name", "").lower() == u.lower()
    us, them = (white, black) if we_white else (black, white)
    winner = g.get("winner")
    if not winner:
        res = "draw"
    else:
        res = "win" if (winner == "white") == we_white else "loss"
    return {
        "res": res, "we_white": we_white,
        "opp": them.get("user", {}).get("name", "?"),
        "opp_rating": them.get("rating"),
        "opp_title": them.get("user", {}).get("title", ""),
        "diff": us.get("ratingDiff"),
        "speed": g.get("speed", "?"),
        "opening": (g.get("opening") or {}).get("name", ""),
        "id": g.get("id", ""),
        "at": g.get("lastMoveAt") or g.get("createdAt"),
    }


# ---------------------------------------------------------------------------
# Rating history chart (server-rendered SVG + hover script)
# ---------------------------------------------------------------------------

SERIES_SLOTS = [("Bullet", "--s1"), ("Blitz", "--s2"), ("Rapid", "--s3")]


def build_chart(u):
    hist = fetch(f"https://lichess.org/api/user/{u}/rating-history", ttl=300)
    if not hist:
        return ""
    by_name = {h.get("name"): h.get("points", []) for h in hist}

    series, all_days = [], set()
    for name, var in SERIES_SLOTS:
        pts = {}
        for y, m, d, r in by_name.get(name, []):
            try:
                pts[datetime.date(y, m + 1, d).toordinal()] = r
            except ValueError:
                continue
        if pts:
            series.append({"name": name, "var": var, "pts": pts})
            all_days.update(pts)
    if not series:
        return ""

    # Drop the first recorded day: early provisional ratings swing wildly
    # and stretch the scale until the real trend is unreadable
    if len(all_days) > 1:
        cutoff = min(all_days) + 1
        pruned = []
        for s in series:
            pts = {day: r for day, r in s["pts"].items() if day >= cutoff}
            if pts:
                pruned.append({**s, "pts": pts})
        if pruned:
            series = pruned
            all_days = set()
            for s in series:
                all_days.update(s["pts"])

    today = datetime.date.today().toordinal()
    first = max(min(all_days), today - MAX_DAYS)
    days = list(range(first, today + 1))
    if len(days) < 2:
        days = [first - 1, first]

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

    grid, step = "", max(25, round(span / 3 / 25) * 25)
    tick = int(lo // step + 1) * step
    while tick < hi:
        gy = round(y(tick), 1)
        grid += (f'<line x1="{PAD_L}" y1="{gy}" x2="{CHART_W - PAD_R}" y2="{gy}" class="grid"/>'
                 f'<text x="{PAD_L - 6}" y="{gy + 3.5}" class="tick" text-anchor="end">{tick}</text>')
        tick += step
    for i in range(0, len(days), max(len(days) // 4, 1)):
        d = datetime.date.fromordinal(days[i])
        grid += (f'<text x="{round(x(i), 1)}" y="{CHART_H - 8}" class="tick" '
                 f'text-anchor="middle">{d.strftime("%b %d")}</text>')

    lines, dots, labels = "", "", ""
    for s in series:
        pts = [(round(x(i), 1), round(y(v), 1)) for i, v in enumerate(s["vals"]) if v is not None]
        if not pts:
            continue
        poly = " ".join(f"{px},{py}" for px, py in pts)
        lines += f'<polyline points="{poly}" style="stroke:var({s["var"]})" class="sline"/>'
        ex, ey = pts[-1]
        labels += (f'<circle cx="{ex}" cy="{ey}" r="3" style="fill:var({s["var"]})"/>'
                   f'<text x="{ex - 4}" y="{ey - 7}" class="slabel" text-anchor="end">{s["name"]}</text>')
        dots += (f'<circle id="dot-{s["name"]}" r="4" style="fill:var({s["var"]})" '
                 f'class="hoverdot" visibility="hidden"/>')

    chart_data = json.dumps({
        "days": [datetime.date.fromordinal(d).strftime("%b %d") for d in days],
        "xs": [round(x(i), 1) for i in range(len(days))],
        "ys": {s["name"]: [round(y(v), 1) if v is not None else None for v in s["vals"]]
               for s in series},
        "series": [{"name": s["name"], "vals": s["vals"]} for s in series],
    })

    legend = "".join(f'<span class="chip" style="background:var({s["var"]})"></span>'
                     f'<span class="lgname">{s["name"]}</span>' for s in series)

    return f"""
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
    tip.innerHTML = rows; tip.style.display = 'block';
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
.controls { display:flex; gap:8px; flex-wrap:wrap; margin-bottom:8px; }
.controls form { display:inline; }
button { font:14px system-ui; padding:6px 14px; border-radius:6px; cursor:pointer;
         border:1px solid var(--line); background:transparent; color:var(--ink); }
button.primary { background:var(--accent); border-color:var(--accent); color:#fff; }
.chipstate { font-size:13px; color:var(--muted); align-self:center; }
.tabs { display:flex; gap:4px; margin-bottom:14px; }
.tab { font-size:14px; padding:5px 12px; border-radius:6px; text-decoration:none;
       color:var(--muted); border:1px solid var(--line); }
.tab.on { color:#fff; background:var(--accent); border-color:var(--accent); }
.msg { font-size:13px; color:var(--accent); margin-bottom:14px; min-height:1px; }
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
.bar { display:flex; gap:2px; height:12px; border-radius:4px; overflow:hidden; margin:6px 0; }
.bar span { display:block; min-width:1px; }
.bar .w { background:var(--good); } .bar .d { background:var(--muted); } .bar .l { background:var(--bad); }
.grid2 { display:grid; grid-template-columns:1fr 1fr; gap:8px 20px; }
.statline { font-size:14px; } .statline .lab { color:var(--muted); }
.statline b { font-variant-numeric:tabular-nums; }
.op { margin:9px 0; } .oprow { display:flex; justify-content:space-between; font-size:14px; }
.opname { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.opsc { color:var(--muted); font-size:13px; flex:none; margin-left:10px; }
table { width:100%; border-collapse:collapse; }
th { text-align:left; color:var(--muted); font-size:12px; text-transform:uppercase;
     letter-spacing:.04em; font-weight:500; padding:4px 8px; }
td { padding:6px 8px; border-top:1px solid var(--line); }
.card a { color:var(--accent); text-decoration:none; }
.win { color:var(--good); font-weight:600; } .loss { color:var(--bad); font-weight:600; }
.mut { color:var(--muted); font-weight:400; font-size:13px; }
.foot { color:var(--muted); font-size:12px; margin-top:18px; }
"""


def wdl_bar(w, d, l):
    t = max(w + d + l, 1)
    return (f'<div class="bar"><span class="w" style="width:{100*w/t:.1f}%"></span>'
            f'<span class="d" style="width:{100*d/t:.1f}%"></span>'
            f'<span class="l" style="width:{100*l/t:.1f}%"></span></div>')


def score_pct(w, d, l):
    t = max(w + d + l, 1)
    return f"{100 * (w + d / 2) / t:.0f}%"


def controls(running):
    if running:
        buttons = ('<form method="post" action="/stop"><button>stop bot</button></form>'
                   '<form method="post" action="/restart"><button>restart bot</button></form>')
    else:
        buttons = '<form method="post" action="/start"><button class="primary">start bot</button></form>'
    buttons += ('<form method="post" action="/update-engine" '
                'onsubmit="return confirm(\'Download the latest engine and restart?\')">'
                '<button>update engine</button></form>'
                '<form method="post" action="/reset-peaks" '
                'onsubmit="return confirm(\'Reset peak ratings to the current ratings?\')">'
                '<button>reset peaks</button></form>')
    chip = "running" if running else "stopped"
    return f'<div class="controls">{buttons}<span class="chipstate">bot: {chip}</span></div>'


def view_toggle(view):
    opts = [("all", "All"), ("bot", "vs Bots"), ("human", "vs Humans")]
    links = "".join(
        f'<a class="tab{" on" if view == k else ""}" href="/?vs={k}">{lbl}</a>'
        for k, lbl in opts)
    return f'<div class="tabs">{links}</div>'


def page(view="all"):
    if view not in ("all", "bot", "human"):
        view = "all"
    u = username()
    running = supervisor.alive()
    if not u:
        return ("<h1>chess-engine bot</h1><p>Waiting for Lichess connection "
                "(no username yet - is LICHESS_BOT_TOKEN set?)</p>" + controls(running))

    esc = html.escape
    user = fetch(f"https://lichess.org/api/user/{u}", ttl=60) or {}
    status = fetch(f"https://lichess.org/api/users/status?ids={u}&withGameIds=true", ttl=15)
    st = status[0] if status else {}
    raw_games = fetch(f"https://lichess.org/api/games/user/{u}?max={NGAMES}&opening=true",
                      {"Accept": "application/x-ndjson"}, ttl=60, ndjson=True) or []
    games = [game_view(g, u) for g in raw_games]

    playing = st.get("playing")            # boolean: is a game in progress
    game_id = st.get("playingId")          # the game's id (withGameIds=true)
    state = ("playing" if playing else "online") if st.get("online") else "offline"
    dot = "dot on" if st.get("online") else "dot"
    now_playing = (f' &middot; <a href="https://lichess.org/{esc(str(game_id))}">watch live</a>'
                   if playing and game_id else "")

    # Rating tiles with trend + local resettable peak
    perfs = user.get("perfs", {})
    peaks = track_peaks({k: perfs.get(k, {}).get("rating")
                         for k in ("bullet", "blitz", "rapid", "classical")
                         if perfs.get(k, {}).get("games")})
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
        peak = peaks.get(key)
        peak_txt = f" &middot; peak {peak}" if peak else ""
        tiles += (f'<div class="tile"><div class="k">{label}</div>'
                  f'<div class="v">{p.get("rating", "-")}{prov} {trend}</div>'
                  f'<small class="mut">{p.get("games", 0)} games{peak_txt}</small></div>')
    c = user.get("count", {})
    tiles += (f'<div class="tile"><div class="k">Games</div>'
              f'<div class="v">{c.get("all", 0)}</div>'
              f'<small class="mut">{c.get("rated", 0)} rated</small></div>')

    chart_svg = build_chart(u)

    # All-time tallies, accumulated once; `sub` is the selected slice
    store = openings_store(u)
    sub = store[view]
    vlabel = {"all": "all-time", "bot": "vs bots", "human": "vs humans"}[view]

    # Recent games filtered to the selected group
    if view != "all":
        games = [gv for gv in games
                 if ("bot" if gv["opp_title"] == "BOT" else "human") == view]

    # ---- Performance card: overall + by color + by speed -------------------
    def color_totals(side):
        w = d = l = 0
        for r in sub[side].values():
            w += r["win"]; d += r["draw"]; l += r["loss"]
        return w, d, l

    ww, wd, wl = color_totals("white")
    bw, bd, bl = color_totals("black")
    w, d, l = ww + bw, wd + bd, wl + bl
    if w + d + l == 0 and view == "all":  # not populated yet: fall back to profile
        w, d, l = c.get("win", 0), c.get("draw", 0), c.get("loss", 0)

    # Current streak from the recent games (newest first)
    streak_n, streak_res = 0, None
    for gv in games:
        if streak_res is None:
            streak_res = gv["res"]
        if gv["res"] == streak_res:
            streak_n += 1
        else:
            break
    streak_txt = (f'{streak_n} {streak_res}{"s" if streak_n != 1 else ""}'
                  if streak_res else "-")

    color_block = (
        f'<div><div class="statline"><span class="lab">as White</span> '
        f'&nbsp;<b>{ww}-{wd}-{wl}</b> <span class="mut">{score_pct(ww,wd,wl)}</span></div>'
        f'{wdl_bar(ww, wd, wl)}</div>'
        f'<div><div class="statline"><span class="lab">as Black</span> '
        f'&nbsp;<b>{bw}-{bd}-{bl}</b> <span class="mut">{score_pct(bw,bd,bl)}</span></div>'
        f'{wdl_bar(bw, bd, bl)}</div>')

    speed_lines = ""
    for s in ("bullet", "blitz", "rapid", "classical"):
        r = sub["speed"].get(s)
        if r:
            speed_lines += (f'<div class="statline"><span class="lab">{s}</span> '
                            f'&nbsp;<b>{r["win"]}-{r["draw"]}-{r["loss"]}</b> '
                            f'<span class="mut">{score_pct(r["win"],r["draw"],r["loss"])}</span></div>')

    perf = f"""
<div class="card">
  <div class="cardhead"><span class="cardtitle">Performance</span>
    <span class="mut">streak: {streak_txt}</span></div>
  <div class="statline"><b>{w}</b> wins &middot; <b>{d}</b> draws &middot; <b>{l}</b> losses
    &middot; {score_pct(w,d,l)} score</div>
  {wdl_bar(w, d, l)}
  <div class="grid2" style="margin-top:10px">{color_block}</div>
  <div style="margin-top:8px">{speed_lines}</div>
  <div class="mut" style="margin-top:6px;font-size:12px">{vlabel} by color / speed</div>
</div>"""

    # ---- Opponents card: by rating strength -------------------------------
    bucket_rows = ""
    for b in BUCKET_ORDER:
        r = sub["buckets"].get(b)
        if r:
            bucket_rows += (f'<div class="op"><div class="oprow">'
                            f'<span class="opname">vs {b}</span>'
                            f'<span class="opsc">{r["n"]}g &middot; {r["win"]}-{r["draw"]}-{r["loss"]} '
                            f'&middot; {score_pct(r["win"],r["draw"],r["loss"])}</span></div>'
                            f'{wdl_bar(r["win"], r["draw"], r["loss"])}</div>')
    # Extremes and favorite/least favorite opponents (min 5 games)
    extra = ""
    bw_ = sub.get("best_win")
    if bw_:
        extra += (f'<div class="statline"><span class="lab">highest rated win</span> '
                  f'&nbsp;<b>{bw_["rating"]}</b> <a href="https://lichess.org/{esc(bw_["id"])}">'
                  f'{esc(bw_["opp"])}</a></div>')
    wl_ = sub.get("worst_loss")
    if wl_:
        extra += (f'<div class="statline"><span class="lab">lowest rated loss</span> '
                  f'&nbsp;<b>{wl_["rating"]}</b> <a href="https://lichess.org/{esc(wl_["id"])}">'
                  f'{esc(wl_["opp"])}</a></div>')
    elig = [(name, r) for name, r in sub.get("opps", {}).items() if r["n"] >= 5]
    if elig:
        def opp_score(kv):
            r = kv[1]
            return (r["win"] + r["draw"] / 2) / r["n"]
        fav = max(elig, key=opp_score)
        nem = min(elig, key=opp_score)
        if fav[0] != nem[0]:
            for lab, (name, r) in (("favorite opponent", fav), ("least favorite", nem)):
                extra += (f'<div class="statline"><span class="lab">{lab}</span> '
                          f'&nbsp;{esc(name)} <b>{r["win"]}-{r["draw"]}-{r["loss"]}</b> '
                          f'<span class="mut">{score_pct(r["win"],r["draw"],r["loss"])} '
                          f'in {r["n"]}g</span></div>')

    opponents = f"""
<div class="card"><div class="cardhead"><span class="cardtitle">Opponents by strength</span>
  <span class="mut">{vlabel}</span></div>
  {bucket_rows or '<div class="mut">no data yet</div>'}
  <div style="margin-top:8px">{extra}</div>
</div>""" if bucket_rows or extra else ""

    # ---- Openings card: split by color (from the selected slice) ----------
    def opening_col(side_tally):
        top = sorted(side_tally.items(), key=lambda kv: -kv[1]["n"])[:6]
        if not top:
            return '<div class="mut">no data yet</div>'
        out = ""
        for fam, r in top:
            out += (f'<div class="op"><div class="oprow"><span class="opname">{esc(fam)}</span>'
                    f'<span class="opsc">{r["n"]}g &middot; {r["win"]}-{r["draw"]}-{r["loss"]}</span></div>'
                    f'{wdl_bar(r["win"], r["draw"], r["loss"])}</div>')
        return out

    total_ops = sum(r["n"] for r in sub["white"].values()) + \
                sum(r["n"] for r in sub["black"].values())

    # Best/worst opening family per color (families with enough games)
    def callout(side_tally):
        ranked = [(fam, r) for fam, r in side_tally.items() if r["n"] >= 10]
        if not ranked:
            return ""
        def sc(r):
            return (r["win"] + r["draw"] / 2) / r["n"]
        best = max(ranked, key=lambda kr: sc(kr[1]))
        worst = min(ranked, key=lambda kr: sc(kr[1]))
        return (f'<div class="statline" style="margin-bottom:6px">'
                f'<span class="up">best</span> {esc(best[0])} '
                f'<span class="mut">{score_pct(best[1]["win"],best[1]["draw"],best[1]["loss"])}</span><br>'
                f'<span class="down">worst</span> {esc(worst[0])} '
                f'<span class="mut">{score_pct(worst[1]["win"],worst[1]["draw"],worst[1]["loss"])}</span></div>')

    openings = f"""
<div class="card"><div class="cardhead"><span class="cardtitle">Openings</span>
  <span class="mut">{vlabel} &middot; {total_ops} games</span></div>
<div class="grid2">
  <div><div class="statline lab" style="margin-bottom:4px">as White</div>
    {callout(sub["white"])}{opening_col(sub["white"])}</div>
  <div><div class="statline lab" style="margin-bottom:4px">as Black</div>
    {callout(sub["black"])}{opening_col(sub["black"])}</div>
</div></div>"""

    # ---- Recent games table -----------------------------------------------
    def when(ms):
        if not ms:
            return ""
        mins = (time.time() - ms / 1000) / 60
        if mins < 60:
            return f"{int(mins)}m ago"
        if mins < 60 * 24:
            return f"{int(mins // 60)}h ago"
        return datetime.datetime.fromtimestamp(ms / 1000).strftime("%b %d")

    rows = ""
    for gv in games[:12]:
        opp_txt = (f'{esc(gv["opp"])} <span class="mut">({gv["opp_rating"]})</span>'
                   if gv["opp_rating"] else esc(gv["opp"]))
        res = gv["res"]
        if gv["diff"] is not None:
            res += f' <span class="mut">{"+" if gv["diff"] >= 0 else ""}{gv["diff"]}</span>'
        opening_short = (gv["opening"].split(":")[0] if gv["opening"] else "")
        rows += (f'<tr><td><a href="https://lichess.org/{esc(gv["id"])}">{esc(gv["speed"])}</a></td>'
                 f'<td>{opp_txt}</td>'
                 f'<td class="mut">{"white" if gv["we_white"] else "black"}</td>'
                 f'<td class="mut">{esc(opening_short)}</td>'
                 f'<td class="{gv["res"] if gv["res"] != "draw" else ""}">{res}</td>'
                 f'<td class="mut">{when(gv["at"])}</td></tr>')
    games_table = f"""
<div class="card"><div class="cardhead"><span class="cardtitle">Recent games</span>
  <span class="mut">{vlabel} &middot; download:
    <a href="/games.pgn?scope=recent">last 60</a> &middot;
    <a href="/games.pgn">all</a></span></div>
<table><tr><th>Speed</th><th>Opponent</th><th>Color</th><th>Opening</th><th>Result</th><th>When</th></tr>
{rows or '<tr><td colspan="6" class="mut">no recent games in this group</td></tr>'}</table></div>"""

    msg = supervisor.status
    supervisor.status = ""  # show once
    msg_html = f'<div class="msg">{esc(msg)}</div>' if msg else ""

    return f"""<h1><a href="https://lichess.org/@/{esc(u)}">{esc(u)}</a></h1>
<div class="sub"><span class="{dot}"></span>{state}{now_playing}
</div>
{controls(running)}
{msg_html}
<div class="tiles">{tiles}</div>
{chart_svg}
{view_toggle(view)}
{perf}
{opponents}
{openings}
{games_table}
<div class="foot">Auto-refreshes every 60s &middot; data from the public Lichess API</div>"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(parsed.query)

        if parsed.path == "/games.pgn":
            self.send_pgn(q.get("scope", ["all"])[0])
            return

        view = "all"
        vs = q.get("vs", ["all"])[0]
        if vs in ("all", "bot", "human"):
            view = vs
        body = f"""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="60"><title>chess-engine bot</title>
<style>{CSS}</style></head><body>{page(view)}
<script>{CHART_JS}</script></body></html>"""
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_pgn(self, scope):
        """Stream the bot's games from Lichess as a PGN download."""
        u = username()
        if not u:
            self.send_error(503, "not connected to lichess yet")
            return
        url = f"https://lichess.org/api/games/user/{u}?opening=true&evals=false"
        name = f"{u}-all-games.pgn"
        if scope == "recent":
            url += "&max=60"
            name = f"{u}-last-60.pgn"
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": UA, "Accept": "application/x-chess-pgn"})
            with urllib.request.urlopen(req, timeout=120) as r:
                data = r.read()
        except Exception as e:
            self.send_error(502, f"lichess export failed: {e}")
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/x-chess-pgn")
        self.send_header("Content-Disposition", f'attachment; filename="{name}"')
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
        elif self.path == "/update-engine":
            threading.Thread(target=update_engine, daemon=True).start()
            supervisor.status = "updating engine..."
        elif self.path == "/reset-peaks":
            reset_peaks()
            supervisor.status = "peak ratings reset to current"
        self.send_response(303)
        self.send_header("Location", "/")
        self.end_headers()

    def log_message(self, *args):
        pass


def shutdown(*_):
    supervisor.stop()
    sys.exit(0)


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    supervisor.start()
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
