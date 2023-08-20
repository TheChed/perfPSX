# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

CFLAGS   = `xml2-config --cflags` -std=c2x -pedantic -Wall -Wno-deprecated-declarations -Os
CFLAGSXML = `xml2-config --cflags --libs`
CC = clang
LIBS = -lm `xml2-config --libs`

SRC = perfPSX.c
OBJ = ${SRC:.c=.o}

all: perfPSX

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: 

xml: xml.c
	${CC} ${CFLAGSXML} -o xml xml.c

config.h:
	cp config.def.h $@

perfPSX: ${OBJ}
	${CC} -o $@ ${OBJ} ${LIBS}

clean:
	rm -f perfPSX ${OBJ} *.log
