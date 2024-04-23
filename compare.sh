#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2024 zamfofex

set -e

if [[ "x$1" = x ]]
then
	echo 'missing revision'
	exit 1
fi

rm -f moonfish
rm -rf compare
mkdir compare

make moonfish
mv moonfish compare

git checkout "$1"

make moonfish
rev="$(git rev-parse --short HEAD)"
mv moonfish compare/"moonfish-$rev"

git checkout -

cd compare

wget -O- https://moonfish.neocities.org/pohl.pgn.xz | xz -d > openings.pgn

cutechess-cli \
	-engine {name,cmd}=moonfish \
	-engine {name,cmd}=moonfish-"$rev" \
	-openings file=openings.pgn order=random \
	-each proto=uci tc=inf/8+0.125 \
	-games 2 -rounds 64 -repeat 2 -maxmoves 256 \
	-sprt elo0=0 elo1=12 alpha=0.05 beta=0.05 \
	-ratinginterval 12 \
	-concurrency 1 \
	-pgnout "games.pgn"
