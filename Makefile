#!/usr/bin/make -f

# these can be overridden using make variables. e.g.
#   make CFLAGS=-O2
#   make install DESTDIR=$(CURDIR)/debian/meters.lv2 PREFIX=/usr
#
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only
PREFIX ?= /usr/local
CFLAGS ?= -Wall -Wno-unused-function
LIBDIR ?= lib

EXTERNALUI?=yes
KXURI?=yes

override CFLAGS += -g $(OPTIMIZATIONS)
BUILDDIR=build/
RW?=robtk/
###############################################################################
LIB_EXT=.so

LV2DIR ?= $(PREFIX)/$(LIBDIR)/lv2
LOADLIBES=-lm

LV2NAME=meters
BUNDLE=meters.lv2

LV2GTK1=needleUI_gtk
LV2GTK2=eburUI_gtk
LV2GTK3=goniometerUI_gtk
LV2GTK4=dpmUI_gtk
LV2GTK5=kmeterUI_gtk
LV2GTK6=mphase2UI_gtk

LV2GUI1=needleUI_gl
LV2GUI2=eburUI_gl
LV2GUI3=goniometerUI_gl
LV2GUI4=dpmUI_gl
LV2GUI5=kmeterUI_gl
LV2GUI6=mphase2UI_gl

MTRGUI=mtr:needle
EBUGUI=mtr:eburui
GONGUI=mtr:goniometerui
DPMGUI=mtr:dpmui
KMRGUI=mtr:kmeterui
MPWGUI=mtr:mphaseui
MP2GUI=mtr:mphase2ui

###############################################################################

LV2UIREQ=
GLUICFLAGS=-I.
GTKUICFLAGS=-I.

UNAME=$(shell uname)
ifeq ($(UNAME),Darwin)
  LV2LDFLAGS=-dynamiclib
  LIB_EXT=.dylib
  UI_TYPE=ui:CocoaUI
  PUGL_SRC=$(RW)pugl/pugl_osx.m
  PKG_LIBS=
  GLUILIBS=-framework Cocoa -framework OpenGL
  BUILDGTK=no
else
  LV2LDFLAGS=-Wl,-Bstatic -Wl,-Bdynamic
  LIB_EXT=.so
  UI_TYPE=ui:X11UI
  PUGL_SRC=$(RW)pugl/pugl_x11.c
  PKG_LIBS=glu gl
  GLUILIBS=-lX11
  GLUICFLAGS+=`pkg-config --cflags glu`
endif

ifeq ($(EXTERNALUI), yes)
  ifeq ($(KXURI), yes)
    UI_TYPE=kx:Widget
    LV2UIREQ+=lv2:requiredFeature kx:Widget;
    override CFLAGS += -DXTERNAL_UI
  else
    LV2UIREQ+=lv2:requiredFeature ui:external;
    override CFLAGS += -DXTERNAL_UI
    UI_TYPE=ui:external
  endif
endif

ifeq ($(BUILDOPENGL)$(BUILDGTK), nono)
  $(error at least one of gtk or openGL needs to be enabled)
endif

targets=$(BUILDDIR)$(LV2NAME)$(LIB_EXT)

ifneq ($(BUILDOPENGL), no)
targets+=$(BUILDDIR)$(LV2GUI1)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GUI2)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GUI3)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GUI4)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GUI5)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GUI6)$(LIB_EXT)
endif

ifneq ($(BUILDGTK), no)
targets+=$(BUILDDIR)$(LV2GTK1)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK2)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK3)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK4)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK5)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK6)$(LIB_EXT)
endif

###############################################################################
# check for build-dependencies

ifeq ($(shell pkg-config --exists lv2 || echo no), no)
  $(error "LV2 SDK was not found")
endif

ifeq ($(shell pkg-config --exists glib-2.0 gtk+-2.0 pango cairo $(PKG_LIBS) || echo no), no)
  $(error "This plugin requires cairo, pango, openGL, glib-2.0 and gtk+-2.0")
endif

ifeq ($(shell pkg-config --exists fftw3f || echo no), no)
  $(error "fftw3f library was not found")
endif

# check for LV2 idle thread
ifeq ($(shell pkg-config --atleast-version=1.4.2 lv2 && echo yes), yes)
  GLUICFLAGS+=-DHAVE_IDLE_IFACE
  GTKUICFLAGS+=-DHAVE_IDLE_IFACE
  LV2UIREQ+=lv2:requiredFeature ui:idleInterface; lv2:extensionData ui:idleInterface;
endif

ifneq ($(MAKECMDGOALS), submodules)
  ifeq ($(wildcard $(RW)robtk.mk),)
    $(warning This plugin needs https://github.com/x42/robtk)
    $(info set the RW environment variale to the location of the robtk headers)
    ifeq ($(wildcard .git),.git)
      $(info or run 'make submodules' to initialize robtk as git submodule)
    endif
    $(error robtk not found)
  endif
endif

override CFLAGS += -fPIC
override CFLAGS += `pkg-config --cflags lv2`

###############################################################################

IM=gui/img/

UIIMGS=$(IM)meter-bright.c $(IM)meter-dark.c $(IM)screw.c
GTKUICFLAGS+=`pkg-config --cflags gtk+-2.0 cairo pango`
GTKUILIBS+=`pkg-config --libs gtk+-2.0 cairo pango`

GLUICFLAGS+=`pkg-config --cflags cairo pango`
GLUILIBS+=`pkg-config --libs cairo pango pangocairo $(PKG_LIBS)`

ifeq ($(GLTHREADSYNC), yes)
  GLUICFLAGS+=-DTHREADSYNC
endif
ifeq ($(GTKRESIZEHACK), yes)
  GLUICFLAGS+=-DUSE_GTK_RESIZE_HACK
  GLUICFLAGS+=$(GTKUICFLAGS)
  GLUILIBS+=$(GTKUILIBS)
endif

DSPSRC=jmeters/vumeterdsp.cc jmeters/iec1ppmdsp.cc \
  jmeters/iec2ppmdsp.cc jmeters/stcorrdsp.cc \
  ebumeter/ebu_r128_proc.cc \
  jmeters/truepeakdsp.cc jmeters/kmeterdsp.cc \
  zita-resampler/resampler.cc zita-resampler/resampler-table.cc

DSPDEPS=$(DSPSRC) jmeters/jmeterdsp.h jmeters/vumeterdsp.h \
  jmeters/iec1ppmdsp.h jmeters/iec2ppmdsp.h \
  jmeters/stcorrdsp.h ebumeter/ebu_r128_proc.h \
  jmeters/truepeakdsp.h jmeters/kmeterdsp.h \
  zita-resampler/resampler.h zita-resampler/resampler-table.h

goniometer_UIDEP=zita-resampler/resampler.cc zita-resampler/resampler-table.cc
goniometer_UISRC=zita-resampler/resampler.cc zita-resampler/resampler-table.cc -DTHREADSYNC

mphase2_UISRC=`pkg-config --cflags --libs fftw3f`

###############################################################################
# build target definitions
default: all

submodule_pull:
	-test -d .git -a .gitmodules -a -f Makefile.git && $(MAKE) -f Makefile.git submodule_pull

submodule_update:
	-test -d .git -a .gitmodules -a -f Makefile.git && $(MAKE) -f Makefile.git submodule_update

submodule_check:
	-test -d .git -a .gitmodules -a -f Makefile.git && $(MAKE) -f Makefile.git submodule_check

submodules:
	-test -d .git -a .gitmodules -a -f Makefile.git && $(MAKE) -f Makefile.git submodules


all: submodule_check $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl $(targets)

$(BUILDDIR)manifest.ttl: lv2ttl/manifest.gui.ttl.in lv2ttl/manifest.gtk.ttl.in lv2ttl/manifest.lv2.ttl.in lv2ttl/manifest.ttl.in Makefile
	@mkdir -p $(BUILDDIR)
	sed "s/@LV2NAME@/$(LV2NAME)/g" \
	    lv2ttl/manifest.ttl.in > $(BUILDDIR)manifest.ttl
ifneq ($(BUILDOPENGL), no)
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@URI_SUFFIX@//g" \
	    lv2ttl/manifest.lv2.ttl.in >> $(BUILDDIR)manifest.ttl
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@UI_TYPE@/$(UI_TYPE)/;s/@LV2GUI1@/$(LV2GUI1)/g;s/@LV2GUI2@/$(LV2GUI2)/g;s/@LV2GUI3@/$(LV2GUI3)/g;s/@LV2GUI4@/$(LV2GUI4)/g;s/@LV2GUI5@/$(LV2GUI5)/g;s/@LV2GUI6@/$(LV2GUI6)/g" \
	    lv2ttl/manifest.gui.ttl.in >> $(BUILDDIR)manifest.ttl
endif
ifneq ($(BUILDGTK), no)
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@URI_SUFFIX@/_gtk/g" \
	    lv2ttl/manifest.lv2.ttl.in >> $(BUILDDIR)manifest.ttl
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@LV2GTK1@/$(LV2GTK1)/g;s/@LV2GTK2@/$(LV2GTK2)/g;s/@LV2GTK3@/$(LV2GTK3)/g;s/@LV2GTK4@/$(LV2GTK4)/g;s/@LV2GTK5@/$(LV2GTK5)/g;s/@LV2GTK6@/$(LV2GTK6)/g" \
	    lv2ttl/manifest.gtk.ttl.in >> $(BUILDDIR)manifest.ttl
endif

$(BUILDDIR)$(LV2NAME).ttl: lv2ttl/$(LV2NAME).ttl.in lv2ttl/$(LV2NAME).lv2.ttl.in lv2ttl/$(LV2NAME).gui.ttl.in Makefile
	@mkdir -p $(BUILDDIR)
	sed "s/@LV2NAME@/$(LV2NAME)/g" \
	    lv2ttl/$(LV2NAME).ttl.in > $(BUILDDIR)$(LV2NAME).ttl
ifneq ($(BUILDGTK), no)
	sed "s/@UI_URI_SUFFIX@/_gtk/;s/@UI_TYPE@/ui:GtkUI/;s/@UI_REQ@//" \
	    lv2ttl/$(LV2NAME).gui.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
endif
ifneq ($(BUILDOPENGL), no)
	sed "s/@UI_URI_SUFFIX@/_gl/;s/@UI_TYPE@/$(UI_TYPE)/;s/@UI_REQ@/$(LV2UIREQ)/" \
	    lv2ttl/$(LV2NAME).gui.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
	sed "s/@URI_SUFFIX@//g;s/@NAME_SUFFIX@//g;s/@DPMGUI@/$(DPMGUI)_gl/g;s/@EBUGUI@/$(EBUGUI)_gl/g;s/@GONGUI@/$(GONGUI)_gl/g;s/@MTRGUI@/$(MTRGUI)_gl/g;s/@KMRGUI@/$(KMRGUI)_gl/g;s/@MPWGUI@/$(MPWGUI)_gl/g;s/@MP2GUI@/$(MP2GUI)_gl/g" \
	  lv2ttl/$(LV2NAME).lv2.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
endif
ifneq ($(BUILDGTK), no)
	sed "s/@URI_SUFFIX@/_gtk/g;s/@NAME_SUFFIX@/ GTK/g;s/@DPMGUI@/$(DPMGUI)_gtk/g;s/@EBUGUI@/$(EBUGUI)_gtk/g;s/@GONGUI@/$(GONGUI)_gtk/g;s/@MTRGUI@/$(MTRGUI)_gtk/g;s/@KMRGUI@/$(KMRGUI)_gtk/g;s/@MPWGUI@/$(MPWGUI)_gtk/g;;s/@MP2GUI@/$(MP2GUI)_gtk/g;" \
	  lv2ttl/$(LV2NAME).lv2.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
endif

$(BUILDDIR)$(LV2NAME)$(LIB_EXT): src/meters.cc $(DSPDEPS) src/ebulv2.cc src/uris.h src/goniometerlv2.c src/goniometer.h src/spectrumlv2.c src/spectr.c src/xfer.c Makefile
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) \
	  -o $(BUILDDIR)$(LV2NAME)$(LIB_EXT) src/$(LV2NAME).cc $(DSPSRC) \
	  -shared $(LV2LDFLAGS) $(LDFLAGS) $(LOADLIBES)

-include $(RW)robtk.mk

$(BUILDDIR)$(LV2GTK1)$(LIB_EXT): $(UIIMGS) src/uris.h gui/needle.c gui/meterimage.c
$(BUILDDIR)$(LV2GTK2)$(LIB_EXT): gui/ebur.c src/uris.h
$(BUILDDIR)$(LV2GTK3)$(LIB_EXT): gui/goniometer.c src/goniometer.h \
    $(goniometer_UIDEP) zita-resampler/resampler.h zita-resampler/resampler-table.h
$(BUILDDIR)$(LV2GTK4)$(LIB_EXT): gui/dpm.c
$(BUILDDIR)$(LV2GTK5)$(LIB_EXT): gui/kmeter.c
$(BUILDDIR)$(LV2GTK6)$(LIB_EXT): gui/mphase2.c src/uri2.h gui/fft.c

$(BUILDDIR)$(LV2GUI1)$(LIB_EXT): $(UIIMGS) src/uris.h gui/needle.c gui/meterimage.c
$(BUILDDIR)$(LV2GUI2)$(LIB_EXT): gui/ebur.c src/uris.h
$(BUILDDIR)$(LV2GUI3)$(LIB_EXT): gui/goniometer.c src/goniometer.h \
    $(goniometer_UIDEP) zita-resampler/resampler.h zita-resampler/resampler-table.h
$(BUILDDIR)$(LV2GUI4)$(LIB_EXT): gui/dpm.c
$(BUILDDIR)$(LV2GUI5)$(LIB_EXT): gui/kmeter.c
$(BUILDDIR)$(LV2GUI6)$(LIB_EXT): gui/mphase2.c src/uri2.h gui/fft.c

###############################################################################
# install/uninstall/clean target definitions

install: all
	install -d $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m755 $(targets) $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -m644 $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl $(DESTDIR)$(LV2DIR)/$(BUNDLE)

uninstall:
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/manifest.ttl
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2NAME).ttl
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2NAME)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI1)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI2)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI3)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI4)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI5)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GUI6)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK1)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK2)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK3)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK4)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK5)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK6)$(LIB_EXT)
	-rmdir $(DESTDIR)$(LV2DIR)/$(BUNDLE)

clean:
	rm -f $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl \
	  $(BUILDDIR)$(LV2NAME)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GUI1)$(LIB_EXT) $(BUILDDIR)$(LV2GUI2)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GUI3)$(LIB_EXT) $(BUILDDIR)$(LV2GUI4)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK1)$(LIB_EXT) $(BUILDDIR)$(LV2GTK2)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK3)$(LIB_EXT) $(BUILDDIR)$(LV2GTK4)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GUI5)$(LIB_EXT) $(BUILDDIR)$(LV2GTK5)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GUI6)$(LIB_EXT) $(BUILDDIR)$(LV2GTK6)$(LIB_EXT)
	rm -rf $(BUILDDIR)*.dSYM
	-test -d $(BUILDDIR) && rmdir $(BUILDDIR) || true

distclean: clean
	rm -f cscope.out cscope.files tags

.PHONY: clean all install uninstall distclean \
        submodule_check submodules submodule_update submodule_pull
