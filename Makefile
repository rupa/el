CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES =
LDFLAGS = -lreadline -lncurses

all: el

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -o el $<

noreadline: el.c
	${CC} ${CFLAGS} -DNO_READLINE -o el $<

.PHONY: clean

clean:
	-rm el
