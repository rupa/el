SHELL = /bin/sh
prefix = /usr/local
exec_prefix = ${prefix}
CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES = -DHAVE_LIBREADLINE
LDFLAGS = -lreadline -lncurses

bindir = ${exec_prefix}/bin
mandir = ${prefix}/man

all: el | el.1
.PHONY: all

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -o $@ $<

noreadline: el.c
	${CC} ${CFLAGS} -o el $<
.PHONY: noreadline

install: el | el.1
	@echo installing executable file to ${bindir}
	@mkdir -p ${bindir}
	@mv el ${bindir}
	@echo installing man page to ${mandir}/man1
	@mkdir -p ${mandir}/man1
	@cp el.1 ${mandir}/man1
.PHONY: install

uninstall:
	@echo removing executable file from ${bindir}
	@rm -f ${bindir}/el
	@echo removing man page from ${mandir}/man1
	@rm -f ${mandir}/man1/el.1
.PHONY: uninstall

clean:
	@echo cleaning
	@-rm -f el
.PHONY: clean

el.1: | el
	help2man -N -n "mnemonic wrapper for EDITOR" -h "-h" -v "-V" -o $@ ./$<

maintainer-clean: clean
	@ echo cleaning man page
	@-rm -f el.1
.PHONY: maintainer-clean
