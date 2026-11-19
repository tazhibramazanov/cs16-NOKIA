#!/bin/sh
# CS 1.6 E52 1.0 — запускает сервер, если он ещё не запущен (для cron раз в 5 минут).
# usage: ./start.sh ПОРТ
cd "$(dirname "$0")"
PORT=${1:-27016}
if ! pgrep -f "cs16_server.py $PORT" >/dev/null 2>&1; then
  nohup python3 cs16_server.py "$PORT" >> server.log 2>&1 &
  echo "сервер запущен на порту $PORT"
else
  echo "сервер уже работает"
fi
