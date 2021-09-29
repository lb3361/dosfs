SHELL=/bin/sh
CFLAGS=-g
DESTDIR=
PREFIX=/usr/local
bindir=${PREFIX}/bin

PGMS = dosfs
OBJS = dosfs.o ff.o ffunicode.o

all: ${PGMS}

clean: FORCE
	-rm ${OBJS} ${PGMS}

dosfs: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS}

${OBJS}: ff.h ffconf.h

install: FORCE
	-mkdir -p "${DESTDIR}${bindir}"
	-cp dosfs "${DESTDIR}${bindir}"
	for n in dosdir dosread doswrite dosmkdir dosdel dosmove dosformat; do \
	  ( cd "${DESTDIR}${bindir}"; ln -s dosfs $$n ) ; done

FORCE:

.PHONY: FORCE