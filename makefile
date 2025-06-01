# moonfish's license: 0BSD
# copyright 2025 zamfofex

.POSIX:
.PHONY: all check clean install
.SUFFIXES:
.SUFFIXES: .c .o

CFLAGS = -O3 -Wall -Wextra -Wpedantic
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
RM = rm -f
EXE = moonfish

# hack for BSD Make
# (ideally, '$^' should be used directly instead)
.ALLSRC ?= $^

tool_obj = tools/utils.o tools/https.o tools/pgn.o tools/lichess.o tools/analyse.o tools/chat.o tools/perft.o
obj = chess.o search.o main.o

moonfish_libs = -lm -pthread -latomic
lichess_libs = -pthread -ltls -lssl -lcrypto -lcjson
analyse_libs = -pthread
chat_libs = -ltls -lssl -lcrypto

all: $(EXE)

$(EXE): $(obj)
lichess analyse chat perft: chess.o tools/utils.o
lichess: tools/lichess.o tools/https.o
analyse: tools/analyse.o tools/pgn.o
chat: tools/chat.o tools/https.o
perft: tools/perft.o

$(obj): moonfish.h
$(tool_obj): moonfish.h tools/tools.h
tools/https.o: tools/https.h

$(EXE) lichess analyse chat perft:
	$(CC) $(LDFLAGS) -o $@ $(.ALLSRC) $($@_libs)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(EXE) perft
	scripts/check.sh

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(DESTDIR)$(BINDIR)/moonfish
	install -D -m 755 lichess $(DESTDIR)$(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(DESTDIR)$(BINDIR)/moonfish-analyse
	install -D -m 755 chat $(DESTDIR)$(BINDIR)/moonfish-chat
