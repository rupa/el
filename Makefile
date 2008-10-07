CC = cc
CFLAGS = -Wall -Wextra -pedantic
DEFINES = 
LDFLAGS = -lreadline

all: el

el: el.c
	${CC} ${CFLAGS} ${DEFINES} ${LDFLAGS} -o el $<

noreadline: el.c
	${CC} ${CFLAGS} -DNO_READLINE -o el $<

clean:
	-rm el
