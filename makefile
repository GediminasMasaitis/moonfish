CFLAGS = -ansi -O3 -Wall -Wextra -Wpedantic

moonfish: *.c moonfish.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o moonfish *.c

play: tools/play.c chess.c moonfish.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -pthread -D_POSIX_C_SOURCE=200809L -o play chess.c tools/play.c
