#!/usr/bin/env bash

# moonfish's license: 0BSD
# copyright 2025 zamfofex

set -ex
set -o shwordsplit || :

# note: this script has been tested to work with Bash and Zsh
# (to use Zsh, invoke it explicitly: 'zsh scripts/compare.sh')

# note: use this script only from the root/project directory

rm -f moonfish
mkdir -p compare
[[ -f compare/openings.fen ]] || wget -O- https://moonfish.cc/pohl.fen.xz | xz -d > compare/openings.fen

dirty=
[[ "x$(git status --porcelain)" = x ]] || dirty=-dirty

rev1="$(git rev-parse --short HEAD)"
make moonfish
mv -f moonfish compare/moonfish-"$rev1$dirty"

git stash
git reset --hard "${1:-main}"

rev2="$(git rev-parse --short HEAD)"
make moonfish
mv -f moonfish compare/moonfish-"$rev2"

[[ "$rev1" = "$rev2" ]] || git reset --hard "$rev1"
[[ "x$dirty" = x ]] || git stash pop

cd compare

cli=c-chess-cli
cli_args="-pgn games.pgn"
format=
protocol=

if ! which "$cli" &> /dev/null
then
	cli=fastchess
	cli_args='-pgnout games.pgn'
	format=format=epd
fi

if ! which "$cli" &> /dev/null
then
	cli=cutechess-cli
	protocol=proto=uci
fi

"$cli" \
	-engine {name=,cmd=./}moonfish-"$rev1$dirty" \
	-engine {name=,cmd=./}moonfish-"$rev2" \
	-each $protocol tc=inf/10+0.1 option.MultiPV=0 \
	-openings $format file=openings.fen order=random \
	-games 2 -rounds 1024 \
	-sprt elo0=0 elo1=12 alpha=0.05 beta=0.05 \
	-concurrency "$(getconf _NPROCESSORS_ONLN)" \
	$cli_args
