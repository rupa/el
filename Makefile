CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES =
LDFLAGS = -lreadline -lncurses

all: el el.1

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -o el $<

noreadline: el.c
	${CC} ${CFLAGS} ${DEFINES} -DNO_READLINE -o el $<

el.1:
	help2man -N -n "mnemonic wrapper for EDITOR" --help-option="-h" --version-option="-V" -o el.1 ./el

.PHONY: clean noreadline

clean:
	-rm el el.1
