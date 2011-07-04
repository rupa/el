CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES =
LDFLAGS = -lreadline -lncurses

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -DHAVE_LIBREADLINE -o el $<

el.1:
	help2man -N -n "mnemonic wrapper for EDITOR" --help-option="-h" --version-option="-V" -o el.1 ./el

noreadline: el.c
	${CC} ${CFLAGS} ${DEFINES} -o el $<

all: el el.1

clean:
	-rm -f el

cleanall: clean
	-rm -f el.1

.PHONY: noreadline all clean cleanall
