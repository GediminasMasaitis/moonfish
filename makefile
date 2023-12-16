# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023 zamfofex

CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $PREFIX/bin

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

src := chess.c search.c main.c

moonfish_cc := $(cc) -pthread -D_POSIX_C_SOURCE=199309L
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L

.PHONY: all clean install

all: moonfish play lichess analyse

moonfish moonfish.exe: moonfish.h $(src)
	$(moonfish_cc) -o $@ $(src)

play: moonfish.h tools/tools.h tools/play.c tools/utils.c chess.c
	$(tools_cc) -o play tools/play.c tools/utils.c chess.c

lichess: tools/tools.h tools/lichess.c tools/utils.c chess.c
	$(tools_cc) -std=c99 -o lichess tools/lichess.c tools/utils.c chess.c -lbearssl -lcjson

analyse: tools/tools.h tools/analyse.c tools/utils.c chess.c
	$(tools_cc) -o analyse tools/analyse.c tools/utils.c chess.c

clean:
	$(RM) moonfish moonfish.exe play lichess analyse
	$(RM) moonfish.c moonfish.c.xz moonfish.sh

install: all
	install -m 755 moonfish $BINDIR/moonfish
	install -m 755 play $BINDIR/moonfish-play
	install -m 755 lichess $BINDIR/moonfish-lichess
	install -m 755 analyse $BINDIR/moonfish-analyse
