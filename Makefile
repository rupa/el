CFLAGS = -Wall -Wextra
DEFINES = -DWITH_READLINE
LDFLAGS = -lreadline

all: el

el: el.c
	cc -g ${CFLAGS} ${DEFINES} ${LDFLAGS} -o el $<

clean:
	-rm foo *.o *.so
