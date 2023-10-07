CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic

cc = $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

ifeq ($(inbuilt_network),yes)

moonfish: moonfish.h *.c net/tanh.c tanh.o
	$(cc) -D_POSIX_C_SOURCE=200809L -o moonfish net/tanh.c tanh.o *.c

tanh.o: tanh.moon
	$(LD) -r -b binary -o tanh.o tanh.moon

tanh.moon: tanh.pickle
	python3 convert.py tanh.pickle

else

moonfish: moonfish.h *.c net/none.c
	$(cc) -o moonfish net/none.c *.c

endif

play: moonfish.h tools/play.c chess.c
	$(cc) -pthread -D_POSIX_C_SOURCE=200809L -o play tools/play.c chess.c

.PHONY: all clean

all: moonfish play

clean:
	$(RM) moonfish play tanh.moon tanh.o
