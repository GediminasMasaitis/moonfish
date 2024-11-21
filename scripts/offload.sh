#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2024 zamfofex

set -ex
set -o shwordsplit || :

undo=no
git add .
git commit -m xxx && undo=yes
git push --force --all tests

commit="$(git rev-parse HEAD)"

[[ "$undo" = yes ]] && git reset HEAD~

url="$(git remote get-url tests)"
ssh="${url%%:*}"
dir="${url#*:}"

ssh "$ssh" -- "cd '$dir/..' && git reset --hard '$commit' && scripts/compare.sh '${1:-main}'"
