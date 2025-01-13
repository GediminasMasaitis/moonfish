# moonfish is licensed under the AGPL (v3 or later)
# copyright 2025 zamfofex

CFLAGS ?= -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
RM ?= rm -f

src = $(.ALLSRC:M*.c)
tools = lichess analyse chat perft

.PHONY: all check clean install

all: moonfish lichess analyse chat

moonfish $(tools):
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(src) -o $@ $(cflags) $(LIBS)

$(tools): moonfish.h tools/tools.h tools/utils.c chess.c

moonfish: moonfish.h threads.h chess.c search.c main.c
moonfish: cflags = -lm -pthread -latomic

lichess: tools/lichess.c tools/https.c tools/https.h
lichess: cflags = -pthread -ltls -lssl -lcrypto -lcjson

analyse: tools/analyse.c tools/pgn.c
analyse: cflags = -pthread

chat: tools/chat.c tools/https.c tools/https.h
chat: cflags = -ltls -lssl -lcrypto

perft: tools/perft.c
perft: cflags =

check: moonfish perft
	scripts/check.sh

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(DESTDIR)$(BINDIR)/moonfish
	install -D -m 755 lichess $(DESTDIR)$(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(DESTDIR)$(BINDIR)/moonfish-analyse
	install -D -m 755 chat $(DESTDIR)$(BINDIR)/moonfish-chat
