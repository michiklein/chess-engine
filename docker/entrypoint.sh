#!/bin/sh
# Inject the Lichess token from the environment into the bridge config,
# then hand off to lichess-bot.
set -e
: "${LICHESS_BOT_TOKEN:?Set LICHESS_BOT_TOKEN to a lichess token with the bot:play scope}"
sed "s/PASTE_YOUR_TOKEN_HERE/${LICHESS_BOT_TOKEN}/" /lichess-bot/config.yml.tmpl > /lichess-bot/config.yml
exec python lichess-bot.py "$@"
