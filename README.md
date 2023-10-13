moonfish
===

[![builds.sr.ht status](https://builds.sr.ht/~zamfofex/moonfish/commits/main.svg)](https://builds.sr.ht/~zamfofex/moonfish/commits/main)
[![Lichess rapid rating](https://lichess-shield.vercel.app/api?username=munfish&format=rapid)](https://lichess.org/@/munfish)

**moonfish** is a chess bot inspired by [sunfish], written in C rather than Python. It is not nearly as complete, but it works well enough!

[sunfish]: <https://github.com/thomasahle/sunfish>

features
---

- very simple NNUE evaluation (same as sunfish)
- alpha/beta pruning search
- cute custom UCI TUI
- custom Lichess integration
- optional threaded search support (enabled by default)

limitations
---

These are things that I plan to fix eventually.

- the bot will never make certain moves
  - no en passant
  - no underpromotion
- the TUI will also prevent you from making those kinds of moves
- the TUI does not detect when the game has ended due to stalemate or checkmate
- no iterative deepening
- no transposition table
- no good move ordering heuristic
- no support for `go infinite` or `go mate`
- no move name or FEN validation (may lead to potential exploits)

building
---

~~~
make
~~~

The UCI TUI, called “play”, may also be built.

~~~
make play
~~~

usage
---

moonfish is a UCI bot, which means you can use it with any UCI program (though see “limitations” above).

However, note that you need an NNUE network in order for it to work! You can take `tanh.pickle` from [sunfish]’s repository, then convert it to moonfish’s format using the `convert.py` script. This should produce a `tanh.moon` file in the same directory, ready to be used for moonfish!

~~~
python3 convert.py tanh.pickle
~~~

Then you can invoke `./moonfish tanh.moon` to start its UCI interface. If you’re using a GUI, make sure to add the path to `tanh.moon` as the sole argument to moonfish.

However, note that moonfish comes with its own UCI TUI, called “play”. You can use it with any UCI engine you’d like! Simply invoke `./play` followed by a time control, then the command of whichever engine you want to play against. The color of your pieces will be decided randomly.

~~~
# (start a game against Stockfish)
./play 1+0 stockfish

# (start a game against Leela)
./play 5+2 lc0

# (start a game against moonfish)
./play 15+10 ./moonfish tanh.moon
~~~

license
---

[GNU][GPL] [AGPL] ([v3][AGPLv3] [or later][GPLv3+]) © zamfofex 2023

[GPL]: <https://www.gnu.org/licenses/>
[AGPL]: <https://www.gnu.org/licenses/why-affero-gpl.html>
[AGPLv3]: <https://www.gnu.org/licenses/agpl-3.0>
[GPLv3+]: <https://www.gnu.org/licenses/gpl-faq.html#VersionThreeOrLater>
