<!-- moonfish is licensed under the AGPL (v3 or later) -->
<!-- copyright 2023, 2024 zamfofex -->

moonfish
===

[![builds.sr.ht status](https://builds.sr.ht/~zamfofex/moonfish/commits/main.svg)](https://builds.sr.ht/~zamfofex/moonfish/commits/main)
[![Lichess rapid rating](https://lichess-shield.vercel.app/api?username=munfish&format=rapid)](https://lichess.org/@/munfish/perf/rapid)
[![Lichess blitz rating](https://lichess-shield.vercel.app/api?username=munfish&format=blitz)](https://lichess.org/@/munfish/perf/blitz)
[![Lichess bullet rating](https://lichess-shield.vercel.app/api?username=munfish&format=bullet)](https://lichess.org/@/munfish/perf/bullet)

**moonfish** is a chess bot inspired by [sunfish], written in C rather than Python. Besides being inspired by it, moonfish doesn’t reuse any code from sunfish, and is written completely from scratch.

You may [play moonfish on Lichess]! You don’t need a Lichess account, just make sure to choose “real time” and select a fast enough time control.

[sunfish]: <https://github.com/thomasahle/sunfish>
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

Note: If your C implementation doesn’t support pthreads, but supports C11 threads, you can pass in `-Dmoonfish_c11_threads`.

Note: If your C implementation doesn’t support threads at all, you can pass in `-Dmoonfish_no_threads`.

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

usage
---

moonfish is a UCI bot, which means you can select it and use it with any UCI program (though see “limitations” above). You can invoke `./moonfish` to start its UCI interface.

However, note that moonfish comes with its own UCI TUIs, called “play” and “analyse”. You can use them with any UCI engine you’d like!

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

`ugi-uci` can be used to let a UGI GUI communicate with a UCI bot, and conversely, `uci-ugi` can be used to let a UCI GUI communicate with a UGI bot.

Simply pass the command of the bot as arguments to either of these tools, and it’ll translate it to be used by the respective GUI.

Note that if the GUI sends a `uci`/`gui` command to the bot that is the same as its protocol, no translation will happen, and the commands will be passed in verbatim between them.


compiling on Windows
---

Clone the repository, then open `moonfish.vcxproj` with Visual Studio. Support for [C11 `<threads.h>`][C11 threads in VS] is required. Only the UCI bot will be compiled, not its tools. (You may use a GUI like [cutechess] to try it.)

Beware that, as of December of 2023, Microsoft, perhaps accidentally, [removed support for C11 threads][missing C11 threads] from Visual Studio but is planning to add it back. Meanwhile, a [workaround][C11 threads workaround] is given in their community support forum.

Note that [MinGW] compilation is also supported.

[cutechess]: <https://github.com/cutechess/cutechess>
[C11 threads in VS]: <https://devblogs.microsoft.com/cppblog/c11-threads-in-visual-studio-2022-version-17-8-preview-2/>
[missing C11 threads]: <https://developercommunity.visualstudio.com/t/Missing-threadsh-in-MSVC-178/10514752>
[C11 threads workaround]: <https://developercommunity.visualstudio.com/t/Missing-threadsh-in-MSVC-178/10514752#T-N10540847>
[MinGW]: <https://mingw-w64.org>

license
---

[AGPL] ([v3 or later][GPLv3+]) © zamfofex 2023, 2024

[AGPL]: <https://gnu.org/licenses/agpl-3.0>
[GPLv3+]: <https://gnu.org/licenses/gpl-faq.html#VersionThreeOrLater>
