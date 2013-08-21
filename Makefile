#!/usr/bin/make -f

# these can be overridden using make variables. e.g.
#   make CFLAGS=-O2
#   make install DESTDIR=$(CURDIR)/debian/meters.lv2 PREFIX=/usr
#
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only
PREFIX ?= /usr/local
CFLAGS ?= -Wall -Wno-unused-function
LIBDIR ?= lib

override CFLAGS += $(OPTIMIZATIONS)

###############################################################################
LIB_EXT=.so

LV2DIR ?= $(PREFIX)/$(LIBDIR)/lv2
LOADLIBES=-lm
LV2NAME=meters
LV2GUI=metersUI
LV2GUI2=eburUI
LV2GUI3=goniometerUI
LV2GUI4=dpmUI
BUNDLE=meters.lv2

UNAME=$(shell uname)
ifeq ($(UNAME),Darwin)
  LV2LDFLAGS=-dynamiclib
  LIB_EXT=.dylib
else
  LV2LDFLAGS=-Wl,-Bstatic -Wl,-Bdynamic
  LIB_EXT=.so
endif

targets=$(LV2NAME)$(LIB_EXT)
targets+=$(LV2GUI)$(LIB_EXT)
targets+=$(LV2GUI2)$(LIB_EXT)
targets+=$(LV2GUI3)$(LIB_EXT)
targets+=$(LV2GUI4)$(LIB_EXT)

# check for build-dependencies
ifeq ($(shell pkg-config --exists lv2 || echo no), no)
  $(error "LV2 SDK was not found")
endif

ifeq ($(shell pkg-config --exists glib-2.0 gtk+-2.0 pango cairo || echo no), no)
  $(error "This plugin requires cairo, pango, glib-2.0 and gtk+-2.0")
endif

override CFLAGS += -fPIC
override CFLAGS += `pkg-config --cflags lv2`

UIDEPS=img/vu.c img/bbc.c img/din.c img/ebu.c img/nor.c img/cor.c img/screw.c
UICFLAGS+=`pkg-config --cflags gtk+-2.0 cairo pango`
UILIBS+=`pkg-config --libs gtk+-2.0 cairo pango`

DSPSRC=jmeters/vumeterdsp.cc jmeters/iec1ppmdsp.cc \
	jmeters/iec2ppmdsp.cc jmeters/stcorrdsp.cc \
	ebumeter/ebu_r128_proc.cc \
	jmeters/truepeakdsp.cc \
	zita-resampler/resampler.cc zita-resampler/resampler-table.cc

DSPDEPS=$(DSPSRC) jmeters/jmeterdsp.h jmeters/vumeterdsp.h \
	jmeters/iec1ppmdsp.h jmeters/iec2ppmdsp.h \
	jmeters/stcorrdsp.h ebumeter/ebu_r128_proc.h \
	jmeters/truepeakdsp.h \
	zita-resampler/resampler.h zita-resampler/resampler-table.h


# build target definitions
default: all

all: manifest.ttl $(LV2NAME).ttl $(targets)

manifest.ttl: manifest.ttl.in
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@LV2GUI@/$(LV2GUI)/g;s/@LV2GUI2@/$(LV2GUI2)/g;s/@LV2GUI3@/$(LV2GUI3)/g;s/@LV2GUI4@/$(LV2GUI4)/g" \
	  manifest.ttl.in > manifest.ttl

$(LV2NAME).ttl: $(LV2NAME).ttl.in
	cat $(LV2NAME).ttl.in > $(LV2NAME).ttl

$(LV2NAME)$(LIB_EXT): $(LV2NAME).cc $(DSPDEPS) ebulv2.cc uris.h goniometerlv2.c goniometer.h spectrumlv2.c
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) \
	  -o $(LV2NAME)$(LIB_EXT) $(LV2NAME).cc $(DSPSRC) \
	  -shared $(LV2LDFLAGS) $(LDFLAGS) $(LOADLIBES)

$(LV2GUI)$(LIB_EXT): ui.c $(UIDEPS) uris.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(UICFLAGS) \
		-o $(LV2GUI)$(LIB_EXT) ui.c \
		-shared $(LV2LDFLAGS) $(LDFLAGS) $(UILIBS)

$(LV2GUI2)$(LIB_EXT): eburui.c $(UIDEPS) \
	common_cairo.h gtkextrbtn.h gtkextcbtn.h gtkextspin.h gtkextpbtn.h gtkextlbl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 $(UICFLAGS) \
		-o $(LV2GUI2)$(LIB_EXT) eburui.c \
		-shared $(LV2LDFLAGS) $(LDFLAGS) $(UILIBS)

$(LV2GUI3)$(LIB_EXT): goniometerui.cc goniometer.h $(UIDEPS) \
	common_cairo.h gtkextdial.h gtkextcbtn.h gtkextscale.h gtkextlbl.h \
	zita-resampler/resampler.cc zita-resampler/resampler-table.cc \
	zita-resampler/resampler.h zita-resampler/resampler-table.h
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(UICFLAGS) $(CXXFLAGS) \
		-o $(LV2GUI3)$(LIB_EXT) goniometerui.cc \
		zita-resampler/resampler.cc zita-resampler/resampler-table.cc \
		-shared $(LV2LDFLAGS) $(LDFLAGS) $(UILIBS)

$(LV2GUI4)$(LIB_EXT): dpmui.c $(UIDEPS) \
	common_cairo.h gtkextdial.h gtkextscale.h gtkextlbl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 $(UICFLAGS) \
		-o $(LV2GUI4)$(LIB_EXT) dpmui.c \
		-shared $(LV2LDFLAGS) $(LDFLAGS) $(UILIBS)

# install/uninstall/clean target definitions

install: all
	install -d $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(LV2NAME)$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m644 manifest.ttl $(LV2NAME).ttl $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(LV2GUI)$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(LV2GUI2)$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(LV2GUI3)$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(LV2GUI4)$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)

uninstall:
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/manifest.ttl
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2NAME).ttl
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2NAME)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI2)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI3)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI4)$(LIB_EXT)
	-rmdir $(DESTDIR)$(LV2DIR)/$(BUNDLE)

clean:
	rm -f manifest.ttl $(LV2NAME).ttl $(LV2NAME)$(LIB_EXT) $(LV2GUI)$(LIB_EXT) $(LV2GUI2)$(LIB_EXT) $(LV2GUI3)$(LIB_EXT) $(LV2GUI4)$(LIB_EXT)

.PHONY: clean all install uninstall
