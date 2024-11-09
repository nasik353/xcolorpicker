PP      ?= g++
PREFIX  ?= /usr/local
NAME    ?= xcolorpicker

CFLAGS  += -std=c++11 -pedantic -Wall -Wextra -Wpedantic -Os
LDFLAGS += -L/usr/lib
LIBS += -lX11

.PHONY: all clean install uninstall

all:
	${PP} ${CFLAGS} ${LDFLAGS} -o ${NAME} ${NAME}.cpp ${LIBS}

clean:
	rm -f ${NAME}

install:
	install -Dm755 ${NAME} ${DESTDIR}${PREFIX}/bin/${NAME}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${NAME}
