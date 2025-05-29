<!-- moonfish's license: 0BSD -->
<!-- copyright 2025 zamfofex -->

moonfish
===

[![builds.sr.ht status](https://builds.sr.ht/~zamfofex/moonfish/commits/main.svg)](https://builds.sr.ht/~zamfofex/moonfish/commits/main)
[![Lichess classical rating](https://lichess-shield.vercel.app/api?username=munfish&format=classical)](https://lichess.org/@/munfish/perf/classical)
[![Lichess rapid rating](https://lichess-shield.vercel.app/api?username=munfish&format=rapid)](https://lichess.org/@/munfish/perf/rapid)
[![Lichess blitz rating](https://lichess-shield.vercel.app/api?username=munfish&format=blitz)](https://lichess.org/@/munfish/perf/blitz)

**moonfish** is a simple chess bot written in C89 (using just a few POSIX and C11 APIs).

You may [play moonfish on Lichess]! You don’t need a Lichess account, just make sure to choose “real time” and select a fast enough time control.

[play moonfish on Lichess]: <https://lichess.org/?user=munfish#friend>

table of contents
---

- [features](#features) and [limitations](#limitations)
- [download](#download)
- compiling from source
  - [compiling on UNIX](#compiling-from-source) (and UNIX clones)
  - [compiling on Plan 9](#compiling-on-9front) (specifically 9front)
  - [compiling on Windows](#compiling-on-windows)
  - [compiling on other systems](#porting-moonfish)
- using moonfish’s tools
  - [using “analyse”](#using-analyse)
  - [using “chat”](#using-chat) (for integrating UCI bots with IRC)
  - [using “lichess”](#using-lichess) (for integrating UCI bots with Lichess)
- [helping improve moonfish!](#contributing-to-moonfish)
- [license](#license-0bsd)

features
---

- simple evaluation based on PSTs
- MCTS (Monte Carlo tree search)
- cute custom UCI TUI
- custom Lichess integration
- custom IRC integration

limitations
---

These are things that might be resolved eventually.

- the TUI does not let you underpromote
- no support for some seldom used UCI features

download
---

Pre‐compiled executables for Linux of moonfish (and some of its tools) may be found at <https://moonfish.cc>

contributing to moonfish
---

Contributions to moonfish are always welcome! Whether you just have thoughts to share, or you want to improve its source code, any kind of help is vastly appreciated!

Contributions (complaints, ideas, thoughts, patches, etc.) may be submitted via email to <zamfofex@twdb.moe> or shared in its IRC channel ([#moonfish] on [Libera Chat]).

- Note: The IRC channel is also bridged to Discord. (Please ask on IRC for a Discord invite.)
- Note: There is also a [#moonfish-miscellany] channel for general off‐topic conversations.

[Libera Chat]: <https://libera.chat>
[#moonfish]: <https://web.libera.chat/#moonfish>
[#moonfish-miscellany]: <https://web.libera.chat/#moonfish-miscellany>

compiling from source
---

Compiling on UNIX (and clones) should be easy! Make sure you have [POSIX Make] (like [GNU Make] or [BSD Make] or [PDP Make]) and a C compiler (like [GCC] or [Clang]), then run the following command.

~~~
make moonfish
~~~

[POSIX Make]: <https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html>
[GNU Make]: <https://gnu.org/software/make/>
[BSD Make]: <https://man.netbsd.org/make.1>
[PDP Make]: <https://frippery.org/make/>
[GCC]: <https://gnu.org/software/gcc/>
[Clang]: <https://clang.llvm.org>

Conversely, you may also invoke your compiler by hand. (Feel free to replace `cc` with your compiler of choice.)

~~~
cc -O3 -o moonfish chess.c search.c main.c -lm -pthread
~~~

By default, moonfish uses C11 `<threads.h>`, but it is possible to have it use POSIX `<pthread.h>` instead by passing `-Dmoonfish_pthreads` to the compiler. (This is necessary if you’re on Mac OS.)

~~~
cc -O3 -Dmoonfish_pthreads -o moonfish chess.c search.c main.c -lm -pthread
~~~

It is also possible to compile moonfish without a dependency on threads by passing `-Dmoonfish_no_threads` to the compiler.

~~~
cc -O3 -Dmoonfish_no_threads -o moonfish chess.c search.c main.c -lm
~~~

Note: It’s also possible to pass arguments to the compiler when using Make by specifying `CFLAGS`:

~~~
make CFLAGS='-O3 -Dmoonfish_pthreads' moonfish
~~~

using “analyse”
---

~~~
make analyse
~~~

“analyse” is a TUI that allows you to analyse chess games with UCI bots.

After compiling and running it, you may use the mouse to click and move pieces around. (So, it requires mouse support from your terminal.)

To analyse a game with a UCI bot, use `./analyse` followed optionally by the UCI options you want to specify, and then the command of whichever bot you want to use for analysis.

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

using “chat”
---

~~~
make chat
~~~

moonfish’s IRC integration, called “chat” may be used to play with a UCI bot through IRC. The bot will connect to a given network through TLS and then start responding to move names such as “e4” written on given channels.

~~~
# have moonfish play on a given channel
./chat -N irc.example.net -C '#my-channel' ./moonfish

# have Stockfish play on the given channels with the given nickname "chess-bot"
./chat -N irc.example.net -C '#my-channel,#other-channel' -M chess-bot stockfish
~~~

It is only possible to connect to networks using TLS. The default nickname is “moonfish”, and it will connect by default to #moonfish on [Libera Chat].

using “lichess”
---

~~~
make lichess
~~~

In order to integrate a UCI bot with [Lichess], it is possible to use “lichess”. Simply write `./lichess` followed by your bot’s command. The Lichess bot token may be specified with the `lichess_token` environment variable.

[Lichess]: <https://lichess.org>

~~~
# (use Stockfish as a Lichess bot)
lichess_token=XXX ./lichess stockfish

# (use moonfish as a Lichess bot)
lichess_token=XXX ./lichess ./moonfish
~~~

compiling on 9front
---

After installing [NPE], each file may be compiled and linked as expected. (Note: A `mkfile` is also provided for covenience.)

[NPE]: <https://git.sr.ht/~ft/npe>

~~~
# (example for x86-64)
6c -I/sys/include/npe -Dmoonfish_plan9 chess.c search.c main.c
6l -o moonfish chess.6 search.6 main.6
moonfish
~~~

compiling on Windows
---

Clone the repository, then open `moonfish.vcxproj` with Visual Studio. Only the UCI bot will be compiled, not its tools. (You may use a GUI like [cutechess] to try it.)

Note that [MinGW] compilation is also supported.

[cutechess]: <https://github.com/cutechess/cutechess>
[MinGW]: <https://mingw-w64.org>

porting moonfish
---

Porting moonfish to a different platform should be a matter of simply providing a “mostly C89‐compliant” C implementation. Of course, moonfish doesn’t make use of *all* C89 features, so it is not necessary to have features that it doesn’t use. [Compiling on 9front](#compiling-on-9front), for example, works through NPE, which provides something close enough to C89 for moonfish to work.

The only pieces of functionality that moonfish optionally depends on that is not specified entirely in C89 are `clock_gettime` and threads. For threads, either pthreads or C11 `<threads.h>` may be used. For `clock_gettime`, it is possible to use `time` instead. These can be configured with the following compile‐time macros.

- default (without explicit options): use `clock_gettime` and C11 `<threads.h>`
- `-Dmoonfish_pthreads`: use pthreads instead of C11 `<threads.h>`
- `-Dmoonfish_no_threads`: disable threads altogether
- `-Dmoonfish_no_clock`: use `time` instead of `clock_gettime` (note: this is highly discouraged because 1 second time granularity is very detrimental in fast games, besides unfortunate reliance on wall‐clock time.)

Thus, moonfish can be compiled within a strict C89 implementation by setting the `moonfish_no_threads` and `moonfish_no_clock` macros during compile time.

license (0BSD)
---

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
