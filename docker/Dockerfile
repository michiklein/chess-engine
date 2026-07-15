# Lichess bot container: builds the engine from source and runs it behind
# the lichess-bot bridge. Designed for one-click start/stop on Docker hosts
# like ZimaOS/CasaOS.
#
#   docker compose up -d      (needs LICHESS_BOT_TOKEN in the environment)

# --- Stage 1: build the engine -------------------------------------------
FROM debian:bookworm-slim AS build
RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ cmake make && \
    rm -rf /var/lib/apt/lists/*
COPY . /src
RUN cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /build --target chess_engine -j

# --- Stage 2: lichess-bot bridge + engine --------------------------------
FROM python:3.12-slim
RUN apt-get update && \
    apt-get install -y --no-install-recommends git ca-certificates && \
    git clone --depth 1 https://github.com/lichess-bot-devs/lichess-bot.git /lichess-bot && \
    pip install --no-cache-dir -r /lichess-bot/requirements.txt && \
    apt-get purge -y git && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*

COPY --from=build /build/chess_engine /engine/chess_engine
COPY docker/config.yml /lichess-bot/config.yml.tmpl
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /lichess-bot
ENTRYPOINT ["/entrypoint.sh"]
