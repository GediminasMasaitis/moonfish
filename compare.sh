#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2024 zamfofex

set -ex
set -o shwordsplit || :

# note: this script has been tested to work with Bash and Zsh
# (to use Zsh, invoke it explicitly: 'zsh compare.sh')

# note: use this script only from the root/project directory

make=gmake
which "$make" &> /dev/null || make=make

rm -f moonfish
mkdir -p compare
[[ -f compare/openings.fen ]] || wget -O- https://moonfish.neocities.org/pohl.fen.xz | xz -d > compare/openings.fen

dirty=
[[ "x$(git status --porcelain)" = x ]] || dirty=-dirty

rev1="$(git rev-parse --short HEAD)"
"$make" moonfish
mv -f moonfish compare/moonfish-"$rev1$dirty"

git stash
git switch "${1:-main}"

rev2="$(git rev-parse --short HEAD)"
"$make" moonfish
mv -f moonfish compare/moonfish-"$rev2"

[[ "$rev1" = "$rev2" ]] || git switch -
[[ "x$dirty" = x ]] || git stash pop

cd compare

cli=c-chess-cli
cli_args="-pgn games.pgn"
format=
protocol=

if ! which "$cli" &> /dev/null
then
	cli=cutechess-cli
	cli_args='-pgnout games.pgn'
	format=format=epd
	protocol=proto=uci
fi

"$cli" \
	-engine {name=,cmd=./}moonfish-"$rev1$dirty" \
	-engine {name=,cmd=./}moonfish-"$rev2" \
	-each $protocol tc=inf/4+0.125 \
	-openings $format file=openings.fen order=random \
	-games 2 -rounds 64 \
	-sprt elo0=0 elo1=12 alpha=0.05 beta=0.05 \
	-concurrency 1 \
	$cli_args
