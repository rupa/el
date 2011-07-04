CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES =
LDFLAGS = -lreadline -lncurses
SRCDIR = .
PREFIX = /usr/local
EXEC_PREFIX = ${PREFIX}

BINDIR = ${EXEC_PREFIX}/bin
MANDIR = ${PREFIX}/man

all: el el.1
.PHONY: all

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -DHAVE_LIBREADLINE -o $@ $<

noreadline: el.c
	${CC} ${CFLAGS} ${DEFINES} -o el $<
.PHONY:

install: el | el.1
	mkdir -p ${BINDIR}
	mkdir -p ${MANDIR}/man1
	mv el ${BINDIR}
	cp el.1 ${MANDIR}/man1
.PHONY: install

uninstall:
	rm -f ${BINDIR}/el
	rm -f ${MANDIR}/man1/el.1
.PHONY: uninstall

clean:
	-rm -f el
.PHONY: clean

el.1: el
	help2man -N -n "mnemonic wrapper for EDITOR" -h "-h" -v "-V" -o $@ ./$<

maintainer-clean: clean
	-rm -f el.1
.PHONY: maintainer-clean
