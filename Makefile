# st - simple terminal
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

SRC = st.c x.c $(LIGATURES_C) $(SIXEL_C)
OBJ = $(SRC:.c=.o)

COMMIT = $$(git rev-parse --short HEAD 2>/dev/null)
PROJECT = st-sx
DISTNAME = $(PROJECT)-$(VERSION)-$(COMMIT)

STLDFLAGS += -lpcre2-32

all: st

config.h:
	cp config.def.h config.h

.c.o:
	$(CC) $(STCFLAGS) -c $<

hb.o: $(LIGATURES_H)
sixel.o: sixel.h sixel_hls.h
sixel_hls.o: sixel_hls.h
st.o: config.h patch/* sixel.h st.h win.h
x.o: arg.h config.h patch/* sixel.h st.h win.h $(LIGATURES_H)

$(OBJ): config.h config.mk

st: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

clean:
	rm -f st $(OBJ) $(PROJECT)*.tar.gz

dist: clean
	mkdir -p $(DISTNAME)
	cp -R FAQ LEGACY TODO LICENSE Makefile README README.md\
		keyboardselect.txt xresources-example st-copyout st.desktop\
		config.mk config.def.h st.info st.1 arg.h st.h win.h st.c x.c\
		hb.* sixel.* sixel_hls.* patch\
		$(DISTNAME)
	tar -cf - $(DISTNAME) | gzip > $(DISTNAME).tar.gz
	rm -rf $(DISTNAME)

install: st
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f st $(DESTDIR)$(PREFIX)/bin
	cp -f st-copyout $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/st
	chmod 755 $(DESTDIR)$(PREFIX)/bin/st-copyout
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < st.1 > $(DESTDIR)$(MANPREFIX)/man1/st.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/st.1
	tic -sx st.info
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	cp -f st.desktop $(DESTDIR)$(PREFIX)/share/applications
	mkdir -p $(DESTDIR)$(ICONPREFIX)
	test -f $(ICONNAME) && test ! -f $(DESTDIR)$(ICONPREFIX)/$(ICONNAME) && cp -f $(ICONNAME) $(DESTDIR)$(ICONPREFIX) || :
	@echo Please see the README file regarding the terminfo entry of st.

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/st
	rm -f $(DESTDIR)$(PREFIX)/bin/st-copyout
	rm -f $(DESTDIR)$(MANPREFIX)/man1/st.1
	rm -f $(DESTDIR)$(PREFIX)/share/applications/st.desktop # desktop-entry patch
	rm -f $(DESTDIR)$(ICONPREFIX)/$(ICONNAME)

.PHONY: all clean dist install uninstall
