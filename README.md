<!-- moonfish's license: 0BSD -->
<!-- copyright 2025 zamfofex -->

moonfish
===

**moonfish** is a simple chess bot written in C89 (using just a few POSIX and C11 APIs).

table of contents
---

- [features](#features)
- [download](#download)
- compiling from source
  - [configuring the compilation](#configuring-the-compilation)
  - [compiling on UNIX](#compiling-from-source) (and UNIX clones)
  - [compiling on Plan 9](#compiling-on-9front) (specifically 9front)
  - [compiling on Windows](#compiling-on-windows)
  - [compiling on Mac OS](#notes-for-mac-os)
  - [compiling on other systems](#porting-moonfish)
- [using moonfish’s tools](#using-moonfishs-tools)
- [helping improve moonfish!](#contributing-to-moonfish)
- [license](#license)

features
---

- simple evaluation based on PSTs
- MCTS (Monte Carlo tree search)
- cute custom TUI for UCI bots
- custom Lichess integration
- custom IRC integration

download
---

Precompiled binaries of moonfish for Linux may be found at <https://moonfish.cc>

contributing to moonfish
---

Contributions to moonfish are always welcome! Whether you just have thoughts or ideas to share or complaints to make, any kind of help is very appreciated!

Please feel free to reach out!

- via email: <zamfofex@twdb.moe>
- via IRC: [#moonfish] on [Libera Chat]
- off‐topic channel: [#moonfish-miscellany]

[Libera Chat]: <https://libera.chat>
[#moonfish]: <https://web.libera.chat/#moonfish>
[#moonfish-miscellany]: <https://web.libera.chat/#moonfish-miscellany>

compiling from source
---

You’ll need a C compiler (like [GCC] or [Clang]) and also [POSIX Make] (like [GNU Make] or [BSD Make] or [PDP Make]).

~~~
make moonfish
~~~

[POSIX Make]: <https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html>
[GNU Make]: <https://gnu.org/software/make/>
[BSD Make]: <https://man.netbsd.org/make.1>
[PDP Make]: <https://frippery.org/make/>
[GCC]: <https://gnu.org/software/gcc/>
[Clang]: <https://clang.llvm.org>

You can also run your compiler manually if you prefer.

~~~
# (feel free to replace 'cc' with your compiler of choice)
cc -O3 -o moonfish chess.c search.c main.c -lm -pthread -latomic
~~~

configuring the compilation
---

**Note:** Consult the makefile for more configuration options.

- **threads**: moonfish uses C11 `<threads.h>` by default
  - `make CPPFLAGS=-Dmoonfish_pthreads` to compile with pthreads instead
  - `make CPPFLAGS=-Dmoonfish_no_threads` to disable threads altogether
- **clock/time**: moonfish uses `clock_gettime` by default
  - `make CPPFLAGS=-Dmoonfish_no_clock` to compile with `time()` instead (note: discouraged)
- **libraries**: moonfish uses `-pthread` and `-latomic` by default
  - `make LIBPTHREAD= LIBATOMIC=` to disable them (respectively)
  - `make LIBPTHREAD=-lpthread` to replace `-pthread` with `-lpthread` (for example)

notes for Mac OS
---

MacOS usually lacks `-latomic` and C11 `<threads.h>`, so you may need to compile with `-Dmoonfish_pthreads` and `LIBATOMIC=`.

~~~
# (configuring the compilation for MacOS)
make CPPFLAGS=-Dmoonfish_pthreads LIBATOMIC= moonfish
~~~

using moonfish’s tools
---

moonfish also has a few tools to use UCI bots in various ways.

- **`analyse`**: TUI for analysing chess games with UCI bots
- **`chat`**: IRC integration for UCI bots
- **`lichess`**: Lichess integration for UCI bots

To compile them, simply run `make` followed by the name of the tool you want to compile.

~~~
make analyse
make chat
make lichess
~~~

Each of them has fairly good `--help` documentation, so you may use that to learn more about them!

~~~
./analyse --help
./chat --help
./lichess --help
~~~

Note that some of them have external dependencies!

- `chat` depends on [LibreSSL] (`-ltls`)
- `lichess` depends on [LibreSSL] (`-ltls`) and [cJSON] (`-lcjson`)

[LibreSSL]: <https://libressl.org>
[cJSON]: <https://github.com/DaveGamble/cJSON>

compiling on 9front
---

You’ll need [NPE]. After that, you can just compile moonfish and its tools with `mk`. (They have no further dependencies on 9front.)

[NPE]: <https://git.sr.ht/~ft/npe>

compiling on Windows
---

Clone the repository, then open `moonfish.vcxproj` with Visual Studio.

- Only the UCI bot will be compiled, not the tools.
- Compiling with [MinGW] is also supported. (In this case, the tools may be compiled too.)
- You may use a GUI like [cutechess] to try it.

[cutechess]: <https://github.com/cutechess/cutechess>
[MinGW]: <https://mingw-w64.org>

porting moonfish
---

Porting moonfish to a different platform should be a matter of simply providing a “mostly C89‐compliant” C implementation. Of course, moonfish doesn’t make use of *all* C89 features, so it is not necessary to have features that it doesn’t use. For example, [compiling on 9front](#compiling-on-9front) works through NPE, which provides something close enough to C89 for moonfish to work.

It is possible to compile moonfish within a strict C89 implementation by defining the `moonfish_no_threads` and `moonfish_no_clock` macros.

license
---

moonfish is released under the [0BSD] license:

> Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
>
> THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

[0BSD]: <https://landley.net/toybox/license.html>
