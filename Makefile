# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

CFLAGS   = `xml2-config --cflags` -std=c2x -pedantic -Wall -Wno-deprecated-declarations -Os
CFLAGSXML = `xml2-config --cflags --libs`
CC = clang
LIBS = -lm `xml2-config --libs`

SRC = PSXprofile.c perfPSX.c
OBJ = ${SRC:.c=.o}

all: PSXprofile

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: 

config.h:
	cp config.def.h $@

PSXprofile: PSXprofile.o
	${CC} -o $@ PSXprofile.o ${LIBS}
perfPSX: perfPSX.o
	${CC} -o $@ perfPSX.o ${LIBS}

clean:
	rm -f perfPSX ${OBJ} *.log
