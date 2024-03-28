<!-- moonfish is licensed under the AGPL (v3 or later) -->
<!-- copyright 2023, 2024 zamfofex -->

moonfish
===

[![builds.sr.ht status](https://builds.sr.ht/~zamfofex/moonfish/commits/main.svg)](https://builds.sr.ht/~zamfofex/moonfish/commits/main)
[![Lichess rapid rating](https://lichess-shield.vercel.app/api?username=munfish&format=rapid)](https://lichess.org/@/munfish/perf/rapid)
[![Lichess blitz rating](https://lichess-shield.vercel.app/api?username=munfish&format=blitz)](https://lichess.org/@/munfish/perf/blitz)
[![Lichess bullet rating](https://lichess-shield.vercel.app/api?username=munfish&format=bullet)](https://lichess.org/@/munfish/perf/bullet)

**moonfish** is a very simple and small chess bot written in C 89 (ANSI C).

You may [play moonfish on Lichess]! You don’t need a Lichess account, just make sure to choose “real time” and select a fast enough time control.

[play moonfish on Lichess]: <https://lichess.org/?user=munfish#friend>

features
---

- simple HCE based on PSTs
- alpha/beta pruning search
- cute custom UCI TUIs
- custom Lichess integration
- threaded search
- custom UGI/UCI translators

limitations
---

These are things that might be fixed eventually.

- the bot will never underpromote
- the TUI will also prevent you from underpromoting
- no transposition table
- no support for `go infinite` or `go mate`
- no move name or FEN validation (may lead to potential exploits)

download
---

Pre‐compiled executables for Linux of moonfish (and its tools) may be found at <https://moonfish.neocities.org>

compiling from source
---

Building on UNIX should be easy. Make sure you have GNU `make` and a C compiler like GCC, then run the following command.

~~~
make moonfish
~~~

Conversely, you may also invoke your compiler by hand. (Feel free to replace `cc` with your compiler of choice.)

Note: If your C implementation doesn’t support pthreads, but supports C11 threads, you may pass in `-Dmoonfish_c11_threads`.

Note: If your C implementation doesn’t support threads at all, you may pass in `-Dmoonfish_no_threads`.

~~~
cc -ansi -O3 -pthread -D_POSIX_C_SOURCE=199309L -o moonfish chess.c search.c main.c
~~~

The two UCI TUIs, called “play” and “analyse”, may also be compiled.

~~~
make play analyse
~~~

The UGI/UCI translators may also be compiled:

~~~
make ugi-uci uci-ugi
~~~

The CLI tools, called “battle” and “ribbon” to (respectively) play a single game between two bots and to play a tournament between any given number of bots may also be compiled.

~~~
make battle ribbon
~~~

All such programs may also be compiled by using the default target, `all`. Note the the Lichess integration requires BearSSL and cJSON. (The other programs require no external libraries.)

~~~
make
~~~

using moonfish
---

moonfish is a UCI bot, which means you may select it and use it with any UCI program (though see “limitations” above). You may invoke `./moonfish` to start its UCI interface.

However, note that moonfish comes with its own UCI TUIs, called “play” and “analyse”. You may use them with any UCI bot you’d like!

using “play” and “analyse”
---

To play against a UCI bot, use `./play` followed by the command of whichever bot you want to play against. The color of your pieces will be decided randomly by default.

~~~
# (start a game against Stockfish)
./play stockfish

# (start a game against Leela)
./play lc0

# (start a game against moonfish)
./play ./moonfish
~~~

To analyse a game with an UCI bot, use `./analyse` followed optionally by the UCI options you want to specify, and thethe command of whichever bot you want to use for analysis. (Though note that moonfish currently does not have analysis capabilities.)

~~~
# (analyse a game using Stockfish)
./analyse stockfish

# (analyse a game using Stockfish, showing win/draw/loss evaluation)
./analyse UCI_ShowWDL=true stockfish

# (analyse a game using Leela)
./analyse lc0

# (analyse a game using Leela, showing win/draw/loss evaluation)
./analyse lc0 --show-wdl
~~~

using “battle”
---

The “battle” CLI may be used to have two UCI or UGI bots play against each other. Each bot has to be specified between brackets (these ones: `[` and `]`).

~~~
./battle [ stockfish ] [ lc0 --preload ] > game.pgn
~~~

The bot specified first will play as white (or p1 for UGI) and the bot specified second will play as black (or p2 for UGI).

The program will write a simplified PGN variant to stdout with the game as it happens. The moves will be in UCI/UGI format, rather than SAN.

The program will also annouce to stderr the start and end of the game.

If your bot’s command requires a `]` to be used as an argument, you may use double brackets instead, like `./battle [[ unusual ] --foo ]] [ stockfish ]`. You may use as many brackets as necessary, you just have to match them when closing.

If your bot’s command starts with a dash, you may precede it by `--`, like `./battle [ -- -unusual ] [ stockfish ]`

A FEN string may be passed in to the `battle` command, like `./battle --fen='...' [ stockfish ] [ lc0 ]`

You may also pass in a time control, with time and increment both in milliseconds, like `./battle [ --time=6000+500 stockfish ] [ ./moonfish ]` Since each bot may have a different time control, this has to be specified within the backets for a specific bot. The default is 15 minutes with ten seconds of increment.

You may also pass in `x` as the first character of the given time control to make it “fixed”, which means that it will be reset after every move. This can be used to set a fixed time per move, for example. For example, `./battle [ --time=1000+0 stockfish ] [ --time --time=x1000+0 ./moonfish ]` will set a countdown clock starting at one second for Stockfish, but allow one second for moonfish for every move.

In order to use a UGI bot, you may also pass in `--protocol=ugi` to that bot, like `./battle [ --protocol=ugi some-bot ] [ stockfish ]`. If both bots are UGI, then the game is not assumed to be chess, and the bot playing as p1 will be queried for the status of the game.

using “ribbon”
---

“ribbon” is a CLI for setting up a round‐robin tournament between multiple bots with the help of the “battle” program mentioned above and a makefile (currently requiring GNU Make).

It will output a makefile to stdout, which may be used by GNU Make to start the tournament between the given bots.

You may use the `-j` option of GNU Make to configure the number of concurrent games to play. Likewise, you may stop a given tournament with `ctrl-c` and continue it by simply running Make again.

You may modify the makefile to prevent certain games from continuing to be played.

Each bot’s command should be passed in as a single argument to `./ribbon`, including potential `--time=...` and `--protocol=...` options to be interpreted by “battle”.

The file name of an opening book may be passed in, and the file should contain one FEN per line. Empty lines and comments starting with `#` are allowed.

In addition, as a matter of convenience, you may specify `--time=...` to “ribbon” itself, which will be passed in verbatim to “battle”.

~~~
# prepare a tournament between Stockfish, Leela and moonfish:
./ribbon stockfish lc0 ./moonfish > makefile

# same as above, but with an argument passed in to Leela:
./ribbon stockfish 'lc0 --preload' ./moonfish > makefile

# same, but with the time control made explicit:
./ribbon --time=18000+1000 stockfish 'lc0 --preload' ./moonfish > makefile

# same, but with a different time control for Stockfish:
# (mind the '--' separating "ribbon" options from the bot options for "battle")
./ribbon --time=18000+1000 -- '--time=6000+0 stockfish' 'lc0 --preload' ./moonfish > makefile

# start the tournament, with at most two games being played at any given time:
make -j2

# concatenate the resulting game PGNs into a single file.
cat *.pgn > result.pgn
~~~

In addition, you may specify the “battle” command to “ribbon”, which must be done if “battle” is not installed on your system as `moonfish-battle`, like `./ribbon --battle=./battle stockfish lc0`

~~~
# prepare a tournament using the locally-built "battle":
./ribbon --battle=./battle stockfish lc0 ./moonfish > makefile

# set up a timeout of thirty seconds for each game:
./ribbon --time=1000+0 --battle='timeout 30 moonfish-battle' stockfish lc0 ./moonfish > makefile

# same as above, but with locally-built "battle":
./ribbon --time=1000+0 --battle='timeout 30 ./battle' stockfish lc0 ./moonfish > makefile

# pick up the tournament again on every failure (possibly due to timeout):
while ! make
do : ; done
~~~

using the UGI/UCI translators
---

`ugi-uci` may be used to let a UGI GUI communicate with a UCI bot, and conversely, `uci-ugi` may be used to let a UCI GUI communicate with a UGI bot.

Simply pass the command of the bot as arguments to either of these tools, and it’ll translate it to be used by the respective GUI.

Note that if the GUI sends a `uci`/`gui` command to the bot that is the same as its protocol, no translation will happen, and the commands will be passed in verbatim between them.

compiling on Windows
---

Clone the repository, then open `moonfish.vcxproj` with Visual Studio. Support for [C11 `<threads.h>`][C11 threads in VS] is required. Only the UCI bot will be compiled, not its tools. (You may use a GUI like [cutechess] to try it.)

Note that [MinGW] compilation is also supported.

[cutechess]: <https://github.com/cutechess/cutechess>
[C11 threads in VS]: <https://devblogs.microsoft.com/cppblog/c11-threads-in-visual-studio-2022-version-17-8-preview-2/>
[MinGW]: <https://mingw-w64.org>

license
---

[AGPL] ([v3 or later][GPLv3+]) © zamfofex 2023, 2024

[AGPL]: <https://gnu.org/licenses/agpl-3.0>
[GPLv3+]: <https://gnu.org/licenses/gpl-faq.html#VersionThreeOrLater>
