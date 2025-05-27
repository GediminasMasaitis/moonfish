#!/usr/bin/env bash

# moonfish's license: 0BSD
# copyright 2025 zamfofex

set -eo pipefail

go()
{
	echo 'setoption name Threads value 1'
	echo "position fen $1"
	echo "go nodes $2"
	while read -r line
	do
		case "$line" in "bestmove "*)
			break
		esac
	done
	echo quit
}

show()
{
	echo "- - - POSITION $2 - - -" >&2
	./perft -F "$3" "$1"
	coproc go "$3" `expr "$1" '*' 4096 + 2048`
	{ ./moonfish | tee /dev/fd/3 3> /dev/fd/3 ; } <&"${COPROC[0]}" 3>&"${COPROC[1]}"
	echo >&2
}

for n in 0 1 2 3 4
do
	echo "= = = DEPTH $n = = =" >&2
	echo >&2
	show "$n" 1 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
	show "$n" 2 'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -'
	show "$n" 3 '8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -'
	show "$n" 4.1 'r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1'
	show "$n" 4.2 'r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1'
	show "$n" 5 'rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8'
	show "$n" 6 'r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10'
	echo >&2
done | tee /dev/stderr | diff scripts/check.txt -
