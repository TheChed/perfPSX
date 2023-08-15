# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

CFLAGS   = -std=c2x -pedantic -Wall -Wno-deprecated-declarations -Os
CC = clang
LIBS = -lm

SRC = perfPSX.c
OBJ = ${SRC:.c=.o}

all: perfPSX

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: 

config.h:
	cp config.def.h $@

perfPSX: ${OBJ}
	${CC} -o $@ ${OBJ} ${LIBS}

clean:
	rm -f perfPSX ${OBJ} *.log
