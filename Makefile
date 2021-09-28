SHELL=/bin/sh
CFLAGS=-g

PGMS = dosfs
OBJS = dosfs.o ff.o ffunicode.o

all: ${PGMS}

clean: FORCE
	-rm ${OBJS} ${PGMS}

dosfs: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS}

${OBJS}: ff.h ffconf.h

FORCE:

.PHONY: FORCE
