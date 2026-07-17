#!/bin/sh
# Manage the Lichess bot container on the server (ZimaOS-friendly).
#
#   ./bot.sh setup lip_yourtoken   store the Lichess token (once)
#   ./bot.sh start                 build if needed and run the bot
#   ./bot.sh stop                  take the bot offline
#   ./bot.sh logs                  follow the bot's log (Ctrl-C to detach)
#   ./bot.sh update                git pull + rebuild + restart the bot
#   ./bot.sh build                 (re)build the image only
#   ./bot.sh down                  remove the container (e.g. when switching
#                                  to a ZimaOS-app-managed container)
#
# The token lives in .env (gitignored, chmod 600) and is read by docker
# compose automatically. DOCKER_CONFIG points at the writable data partition
# because the OS root filesystem is read-only.
set -e
cd "$(dirname "$0")"

DC="sudo DOCKER_CONFIG=/DATA/.docker docker compose"

case "$1" in
    setup)
        if [ -z "$2" ]; then
            echo "usage: ./bot.sh setup lip_yourtoken" >&2
            exit 1
        fi
        printf 'LICHESS_BOT_TOKEN=%s\n' "$2" > .env
        chmod 600 .env
        echo "token stored in .env - now run: ./bot.sh start"
        ;;
    start)   $DC up -d --build ;;
    stop)    $DC stop ;;
    logs)    sudo docker logs -f chess-lichess-bot ;;
    update)  git pull && $DC up -d --build ;;
    build)   $DC build ;;
    down)    $DC down ;;
    *)
        grep '^#   ' "$0" | sed 's/^# //'
        exit 1
        ;;
esac
