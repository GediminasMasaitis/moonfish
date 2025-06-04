# moonfish's license: 0BSD
# copyright 2025 zamfofex

.POSIX:
.PHONY: all check clean install
.SUFFIXES:
.SUFFIXES: .c .o

# configurable variables (note: not exhaustive!)
CFLAGS = -O3 -Wall -Wextra -Wpedantic
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
RM = rm -f
LD = $(CC)

# configurable libraries
LIBM = -lm
LIBPTHREAD = -pthread
LIBATOMIC = -latomic
LIBTLS = -ltls -lssl -lcrypto
LIBCJSON = -lcjson

# further configurable variables
moonfish_libs = $(LIBM) $(LIBPTHREAD) $(LIBATOMIC)
lichess_libs = $(LIBPTHREAD) $(LIBTLS) $(LIBCJSON)
analyse_libs = $(LIBPTHREAD)
chat_libs = $(LIBTLS)
line_libs = $(LIBPTHREAD)

# hack for BSD Make
# (ideally, '$^' should be used directly instead)
.ALLSRC ?= $^

tool_obj = tools/utils.o tools/https.o tools/pgn.o tools/lichess.o tools/analyse.o tools/chat.o tools/line.o tools/perft.o
obj = chess.o search.o main.o

all: moonfish lichess analyse chat line

moonfish: $(obj)
lichess analyse chat line perft: chess.o tools/utils.o
lichess: tools/lichess.o tools/https.o
analyse: tools/analyse.o tools/pgn.o
chat: tools/chat.o tools/https.o
line: tools/line.o chess.o
perft: tools/perft.o

$(obj): moonfish.h
$(tool_obj): moonfish.h tools/tools.h
tools/https.o: tools/https.h

moonfish lichess analyse chat line perft:
	$(LD) $(LDFLAGS) -o $@ $(.ALLSRC) $($@_libs)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

check: moonfish perft
	scripts/check.sh

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(DESTDIR)$(BINDIR)/moonfish
	install -D -m 755 lichess $(DESTDIR)$(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(DESTDIR)$(BINDIR)/moonfish-analyse
	install -D -m 755 chat $(DESTDIR)$(BINDIR)/moonfish-chat
	install -D -m 755 line $(DESTDIR)$(BINDIR)/moonfish-line
