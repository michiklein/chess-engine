# Chess Engine

A chess engine written in C++17, playing on Lichess as [kleiniBOT](https://lichess.org/@/kleiniBOT).

## Build

```bash
./build.sh
```

The binary is fully self-contained: the opening book is embedded at build
time from `src/eco.pgn` (see `scripts/embed_book.py`). An `eco.pgn` placed
next to the executable overrides the built-in book. Book moves are weighted
by real master-game frequency, not just how many named lines an opening has
— see `scripts/update_book.py`.

### Windows / macOS distribution builds

```bash
./scripts/build_windows.sh   # needs mingw-w64; outputs dist/windows/chess_engine.exe
./scripts/build_macos.sh     # universal (arm64 + x86_64); outputs dist/macos/chess_engine
```

## Play

```bash
./build/chess_engine    # UCI engine for chess GUIs
```

Moves are entered in algebraic notation: `e4`, `Nf3`, `Bxc5`, `O-O`, or coordinate style: `e2e4`.

## Play on Lichess (Docker)

The engine runs behind [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot)
in a single container, with a status dashboard (ratings, game history,
start/stop/update-engine buttons) on port 8087. CI publishes the container
image and a standalone engine binary on every push to `main`.

**ZimaOS / CasaOS** (recommended for those hosts — no local build needed):
import `zimaos-app.yml` as a customized app (App Store → + → Import), set
`LICHESS_BOT_TOKEN` to a token with the `bot:play` scope, and install. The
app pulls the CI-built image from `ghcr.io/<owner>/chess-engine`.

**Any other Docker host**, via `bot.sh`:

```bash
./bot.sh setup <lichess-token>   # stores the token in .env (once)
./bot.sh start                   # build if needed, bot goes online
./bot.sh stop                    # bot goes offline
./bot.sh logs                    # follow the bot's log
./bot.sh update                  # pull + rebuild + restart + prune (compose path)
./bot.sh update-app              # pull the CI image only (ZimaOS-app path);
                                  # then restart the app from the dashboard
```

Bot settings (time controls, matchmaking, greetings) live in `docker/config.yml`.

Once running, open `http://<host>:8087` for the dashboard: ratings with
resettable peaks, a rating-history chart with engine-update markers,
performance/opponents/openings broken down by bots-vs-humans, and an
**update engine** button that hot-swaps in the latest CI-built binary without
touching the container.

### Automatic container updates (Watchtower)

For hosts where the ZimaOS-style update button doesn't reliably pick up new
`:latest` images, run Watchtower once — it watches only the bot container and
recreates it whenever CI publishes a new image:

```bash
sudo docker run -d --name watchtower --restart unless-stopped \
  -v /var/run/docker.sock:/var/run/docker.sock \
  containrrr/watchtower --cleanup --interval 300 kleinibot
```

## Test

```bash
cd build && ctest              # perft: validates move generation against known node counts
scripts/selfplay_check.py      # a full game, validated move-by-move with python-chess
scripts/endgame_tests.py       # basic mate conversions (KR-K, KQ-K, KP-K, KB-K draw)
scripts/bench.py               # fixed-position node/nps benchmark, for comparing builds
```

Engine changes that affect playing strength are validated with an A/B match
before shipping, not just gut feel:

```bash
scripts/play_stockfish.py --stockfish <old-binary> --games 30 --tc 60+0.6
```

## Project Structure

```
src/
├── types.h              # core types, bitboard utilities
├── board.cpp/h          # board state, make/unmake move
├── movegen.cpp/h        # move generation
├── search.cpp/h         # alpha-beta search, evaluation
├── opening_book.cpp/h   # weighted opening book
├── eco_book.cpp/h       # generated: eco.pgn embedded into the binary
├── uci.cpp/h            # UCI protocol
tests/
├── perft.cpp             # move generation correctness tests
scripts/
├── update_book.py        # rebuild eco.pgn, weighted by real master-game frequency
├── embed_book.py          # embed eco.pgn into eco_book.cpp
├── bench.py, endgame_tests.py, selfplay_check.py, play_stockfish.py
├── build_windows.sh, build_macos.sh
docker/
├── Dockerfile             # engine + lichess-bot bridge + dashboard
├── status_server.py       # dashboard: stats, start/stop, update-engine
├── config.yml, entrypoint.sh
.github/workflows/
├── docker-image.yml       # CI: publishes the ghcr.io image + engine-latest release
```
