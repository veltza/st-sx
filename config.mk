# st version
VERSION = 0.9.3

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man
ICONPREFIX = $(PREFIX)/share/pixmaps
ICONNAME = st.png

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

PKG_CONFIG = pkg-config

# alpha
XRENDER = -lXrender

# ligatures (you need to comment out the four lines below if ligatures are
# permanently disabled in config.h)
LIGATURES_C = hb.c
LIGATURES_H = hb.h
LIGATURES_INC = `$(PKG_CONFIG) --cflags harfbuzz`
LIGATURES_LIBS = `$(PKG_CONFIG) --libs harfbuzz`

# sixel
SIXEL_C = sixel.c sixel_hls.c

# includes and libs
INCS = -I$(X11INC) \
       `$(PKG_CONFIG) --cflags fontconfig` \
       `$(PKG_CONFIG) --cflags freetype2` \
       `$(PKG_CONFIG) --cflags imlib2` \
       $(LIGATURES_INC)
LIBS = -L$(X11LIB) -lm -lX11 -lutil -lXft -lgd $(LIBRT) ${XRENDER} ${XCURSOR} ${PROCSTAT}\
       `$(PKG_CONFIG) --libs fontconfig` \
       `$(PKG_CONFIG) --libs freetype2` \
       `$(PKG_CONFIG) --libs imlib2` \
       $(LIGATURES_LIBS)
LIBRT = -lrt

# flags
STCPPFLAGS = -DVERSION=\"$(VERSION)\" -DICON=\"$(ICONPREFIX)/$(ICONNAME)\" -D_XOPEN_SOURCE=600
STCFLAGS = $(INCS) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
STLDFLAGS = $(LIBS) $(LDFLAGS)

# FreeBSD:
#CPPFLAGS = -D_FREEBSD_SOURCE -D__BSD_VISIBLE
#X11INC = /usr/local/include
#X11LIB = /usr/local/lib
#PROCSTAT = -lprocstat

# OpenBSD:
#CPPFLAGS = -D_BSD_SOURCE
#LIBRT =
#MANPREFIX = ${PREFIX}/man

# compiler and linker
# CC = c99
