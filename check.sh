#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2024 zamfofex

set -e

positions=("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -" "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -" "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10")

diff - <(
	echo 'checking starting position' >&2
	for n in {1..5}
	do ./perft -N"$n"
	done
	
	for f in "${positions[@]}"
	do
		echo "checking '$f'" >&2
		for n in {1..4}
		do ./perft -N"$n" -F"$f"
		done
	done
) <<END
perft 1: 20
perft 2: 400
perft 3: 8902
perft 4: 197281
perft 5: 4865609
perft 1: 48
perft 2: 2039
perft 3: 97862
perft 4: 4085603
perft 1: 14
perft 2: 191
perft 3: 2812
perft 4: 43238
perft 1: 6
perft 2: 264
perft 3: 9467
perft 4: 422333
perft 1: 6
perft 2: 264
perft 3: 9467
perft 4: 422333
perft 1: 44
perft 2: 1486
perft 3: 62379
perft 4: 2103487
perft 1: 46
perft 2: 2079
perft 3: 89890
perft 4: 3894594
END
