#!/usr/bin/env bash

# moonfish's license: 0BSD
# copyright 2025 zamfofex

set -ex
set -o shwordsplit || :

undo=no
git add .
git commit -m xxx && undo=yes
git push --force --all tests

rev1="$(git rev-parse HEAD)"

[[ "$undo" = yes ]] && git reset HEAD~

rev2="$(git rev-parse "${1:-main}")"

url="$(git remote get-url tests)"
ssh="${url%%:*}"
dir="${url#*:}"

ssh "$ssh" -- "cd '$dir/..' && git reset --hard '$rev1' && scripts/compare.sh '$rev2'"
