CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic

cc = $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

ifeq ($(inbuilt_network),yes)

moonfish: moonfish.h *.c net/tanh.c tanh.o
	$(cc) -D_POSIX_C_SOURCE=200809L -DMOONFISH_INBUILT_NET -o moonfish net/tanh.c tanh.o *.c

tanh.o: tanh.moon
	$(LD) -r -b binary -o tanh.o tanh.moon

tanh.moon: tanh.pickle
	python3 convert.py tanh.pickle

else

moonfish: moonfish.h *.c
	$(cc) -o moonfish *.c

endif

play: moonfish.h tools/tools.h tools/play.c tools/utils.c chess.c
	$(cc) -pthread -D_POSIX_C_SOURCE=200809L -o play tools/play.c tools/utils.c chess.c

lichess: tools/tools.h tools/lichess.c tools/utils.c tools/play.c
	$(cc) -pthread -D_POSIX_C_SOURCE=200809L -std=c99 -o lichess tools/lichess.c tools/utils.c -lbearssl -lcjson

.PHONY: all clean

all: moonfish play lichess

clean:
	$(RM) moonfish play lichess tanh.moon tanh.o
