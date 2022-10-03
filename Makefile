UNAME := $(shell uname)
CFLAGS += -g -std=c99 -Wall -Wextra -pedantic
OBJS = em24.o 
INSTALL = /usr/bin/install
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include

LIBS = -lm -lmodbus -lmosquitto
ifneq ($(UNAME), Darwin)
LIBS += -luuid
endif

all: em24

em24: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o em24

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean install uninstall
clean:
	@rm -f *.o
	@rm -f em24

install: em24
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) em24 $(DESTDIR)$(bindir)

uninstall:
	@rm -f $(DESTDIR)$(bindir)/em24
