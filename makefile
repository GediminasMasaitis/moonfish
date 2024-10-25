# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
RM ?= rm -f

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: all clean install uninstall

all: moonfish play lichess analyse chat

moonfish: moonfish.h chess.c search.c main.c
	$(cc) $(filter %.c,$^) -D_POSIX_C_SOURCE=199309L -o $@ -lm

%: moonfish.h tools/tools.h tools/utils.c chess.c tools/%.c
	$(cc) $(filter %.c,$^) -D_POSIX_C_SOURCE=200809L -o $@ $(cflags)

play analyse: cflags := -pthread
analyse: tools/pgn.c
lichess chat: tools/https.c
lichess: cflags := -ltls -lssl -lcrypto -lcjson
chat: cflags := -ltls -lssl -lcrypto
learn: search.c
learn: cflags := -Dmoonfish_learn -lm

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(BINDIR)/moonfish
	install -D -m 755 play $(BINDIR)/moonfish-play
	install -D -m 755 lichess $(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(BINDIR)/moonfish-analyse
	install -D -m 755 chat $(BINDIR)/moonfish-chat

uninstall:
	$(RM) $(BINDIR)/moonfish $(BINDIR)/moonfish-*
