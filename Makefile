#!/usr/bin/make -f

# these can be overridden using make variables. e.g.
#   make CFLAGS=-O2
#   make install DESTDIR=$(CURDIR)/debian/meters.lv2 PREFIX=/usr
#
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -fomit-frame-pointer -O3 -fno-finite-math-only -DNDEBUG
PREFIX ?= /usr/local
CFLAGS ?= -Wall -Wno-unused-function
LIBDIR ?= lib
STRIP  ?= strip

EXTERNALUI?=yes
BUILDGTK?=no
KXURI?=yes

override CFLAGS += -g -fvisibility=hidden $(OPTIMIZATIONS)
BUILDDIR=build/
OBJDIR=obj/
APPBLD=x42/
RW?=robtk/
meters_VERSION?=$(shell git describe --tags HEAD | sed 's/-g.*$$//;s/^v//' || echo "LV2")
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
LV2GTK6=phasewheelUI_gtk
LV2GTK7=stereoscopeUI_gtk
LV2GTK8=dr14meterUI_gtk
LV2GTK9=sdhmeterUI_gtk

LV2GUI1=needleUI_gl
LV2GUI2=eburUI_gl
LV2GUI3=goniometerUI_gl
LV2GUI4=dpmUI_gl
LV2GUI5=kmeterUI_gl
LV2GUI6=phasewheelUI_gl
LV2GUI7=stereoscopeUI_gl
LV2GUI8=dr14meterUI_gl
LV2GUI9=sdhmeterUI_gl

MTRGUI=mtr:needle
EBUGUI=mtr:eburui
GONGUI=mtr:goniometerui
DPMGUI=mtr:dpmui
KMRGUI=mtr:kmeterui
MPWGUI=mtr:phasewheelui
SFSGUI=mtr:stereoscopeui
DRMGUI=mtr:dr14meterui
SDHGUI=mtr:sdhmeterui

###############################################################################

LV2UIREQ=
GLUICFLAGS=-I.
GTKUICFLAGS=-I.

UNAME=$(shell uname)
ifeq ($(UNAME),Darwin)
  LV2LDFLAGS=-dynamiclib
  LIB_EXT=.dylib
  EXE_EXT=
  UI_TYPE=ui:CocoaUI
  PUGL_SRC=$(RW)pugl/pugl_osx.m
  PKG_LIBS=
  GLUILIBS=-framework Cocoa -framework OpenGL
  BUILDGTK=no
  STRIPFLAGS=-u -r -arch all -s $(RW)lv2syms
else
  LV2LDFLAGS=-Wl,-Bstatic -Wl,-Bdynamic -Wl,--as-needed -pthread
  LIB_EXT=.so
  EXE_EXT=
  UI_TYPE=ui:X11UI
  PUGL_SRC=$(RW)pugl/pugl_x11.c
  PKG_LIBS=glu gl
  GLUILIBS=-lX11
  GLUICFLAGS+=`pkg-config --cflags glu` -pthread
  STRIPFLAGS=-s
endif

ifneq ($(XWIN),)
  CC=$(XWIN)-gcc
  CXX=$(XWIN)-g++
  LV2LDFLAGS=-Wl,-Bstatic -Wl,-Bdynamic -Wl,--as-needed -lpthread
  LIB_EXT=.dll
  EXE_EXT=.exe
  PUGL_SRC=$(RW)pugl/pugl_win.cpp
  PKG_LIBS=
  GLUILIBS=-lws2_32 -lwinmm -lopengl32 -lglu32 -lgdi32 -lcomdlg32 -lpthread
  BUILDGTK=no
  GLUICFLAGS=-I.
  override LDFLAGS += -static-libgcc -static-libstdc++
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
targets+=$(BUILDDIR)meters_glui$(LIB_EXT)
endif

ifneq ($(BUILDGTK), no)
targets+=$(BUILDDIR)$(LV2GTK1)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK2)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK3)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK4)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK5)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK6)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK7)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK8)$(LIB_EXT)
targets+=$(BUILDDIR)$(LV2GTK9)$(LIB_EXT)
PKG_LIBS+=gtk+-2.0
endif

###############################################################################
# check for build-dependencies

ifeq ($(shell pkg-config --exists lv2 || echo no), no)
  $(error "LV2 SDK was not found")
endif

ifeq ($(shell pkg-config --exists glib-2.0 pango cairo $(PKG_LIBS) || echo no), no)
  $(error "These plugins requires $(PKG_LIBS) cairo pango glib-2.0")
endif

# TODO jack-wrapper (not enabled by default, yet
# need jack and lv2 > =1.4.2 (idle API)
#
#ifeq ($(shell pkg-config --exists jack || echo no), no)
#  $(warning *** libjack from http://jackaudio.org is required)
#  $(error   Please install libjack-dev or libjack-jackd2-dev)
#endif

ifneq ($(shell test -f fftw-3.3.4/.libs/libfftw3f.a || echo no), no)
  FFTW=-Ifftw-3.3.4/api fftw-3.3.4/.libs/libfftw3f.a -lm
else
  ifeq ($(shell pkg-config --exists fftw3f || echo no), no)
    $(error "fftw3f library was not found")
  endif
  FFTWA=`pkg-config --variable=libdir fftw3f`/libfftw3f.a
  ifeq ($(shell test -f $(FFTWA) || echo no), no)
    FFTWA=`pkg-config --libs fftw3f`
  endif
  $(warning "**********************************************************")
  $(warning "           the fftw3 library is not thread-safe           ")
  $(warning "**********************************************************")
  $(info "These plugins may cause crashes when used in a plugin-host")
  $(info "where libfftw3f symbols are mapped in the global namespace.")
  $(info "Neither these plugins nor the host has control over possible")
  $(info "other plugins calling the fftw planner simultaneously.")
  $(info "Consider statically linking these plugins against a custom build")
  $(info "of libfftw3f.a built with -fvisibility=hidden to avoid this issue.")
  $(warning "")
  $(warning "**********************************************************")
  $(warning "     run   ./static_fft.sh    prior to make to do so.     ")
  $(warning "**********************************************************")
  $(warning "")
  $(eval FFTW=`pkg-config --cflags fftw3f` $(FFTWA) -lm)
endif
export FFTW

# check for LV2 idle thread
ifeq ($(shell pkg-config --atleast-version=1.4.2 lv2 && echo yes), yes)
  GLUICFLAGS+=-DHAVE_IDLE_IFACE
  GTKUICFLAGS+=-DHAVE_IDLE_IFACE
  LV2UIREQ+=lv2:requiredFeature ui:idleInterface; lv2:extensionData ui:idleInterface;
endif

# check for lv2_atom_forge_object  new in 1.8.1 deprecates lv2_atom_forge_blank
ifeq ($(shell pkg-config --atleast-version=1.8.1 lv2 && echo yes), yes)
  override CFLAGS += -DHAVE_LV2_1_8
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

ifeq ($(XWIN),)
override CFLAGS += -fPIC
else
override CFLAGS += -DPTW32_STATIC_LIB
override CXXFLAGS += -DPTW32_STATIC_LIB
endif
override CFLAGS += `pkg-config --cflags lv2` -DVERSION="\"$(meters_VERSION)\""
override CXXFLAGS += -DVERSION="\"$(meters_VERSION)\""

###############################################################################

IM=gui/img/

UIIMGS=$(IM)meter-bright.c $(IM)meter-dark.c $(IM)screw.c
GTKUICFLAGS+=`pkg-config --cflags gtk+-2.0 cairo pango`
GTKUILIBS+=`pkg-config --libs gtk+-2.0 cairo pango`

GLUICFLAGS+=`pkg-config --cflags cairo pango`
GLUILIBS+=`pkg-config --libs $(PKG_UI_FLAGS) cairo pangocairo pango $(PKG_LIBS)`
ifneq ($(XWIN),)
GLUILIBS+=-lpthread -lusp10
endif

GLUICFLAGS+=$(LIC_CFLAGS)
GLUILIBS+=$(LIC_LOADLIBES)

GLUICFLAGS+=-DUSE_GUI_THREAD
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
  jmeters/msppmdsp.cc ebumeter/ebu_r128_proc.cc \
  jmeters/truepeakdsp.cc jmeters/kmeterdsp.cc \
  zita-resampler/resampler.cc zita-resampler/resampler-table.cc

DSPDEPS=$(DSPSRC) jmeters/jmeterdsp.h jmeters/vumeterdsp.h \
  jmeters/iec1ppmdsp.h jmeters/iec2ppmdsp.h jmeters/msppmdsp.h \
  jmeters/stcorrdsp.h ebumeter/ebu_r128_proc.h \
  jmeters/truepeakdsp.h jmeters/kmeterdsp.h \
  zita-resampler/resampler.h zita-resampler/resampler-table.h

goniometer_UIDEP=zita-resampler/resampler.cc zita-resampler/resampler-table.cc
goniometer_UISRC=zita-resampler/resampler.cc zita-resampler/resampler-table.cc

$(eval phasewheel_UISRC=$(value FFTW))
$(eval stereoscope_UISRC=$(value FFTW))

$(eval meters_UISRC=$(value FFTW))
meters_UISRC+=zita-resampler/resampler.cc zita-resampler/resampler-table.cc

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

jackapps: \
	$(APPBLD)x42-dr14 \
	$(APPBLD)x42-ebur128 \
	$(APPBLD)x42-goniometer \
	$(APPBLD)x42-histogram \
	$(APPBLD)x42-k20rms \
	$(APPBLD)x42-phase-correlation \
	$(APPBLD)x42-phasewheel \
	$(APPBLD)x42-spectrum30 \
	$(APPBLD)x42-stereoscope \
	$(APPBLD)x42-truepeakrms \
	$(APPBLD)x42-meter-collection

$(BUILDDIR)manifest.ttl: lv2ttl/manifest.gui.ttl.in lv2ttl/manifest.gtk.ttl.in lv2ttl/manifest.lv2.ttl.in lv2ttl/manifest.ttl.in Makefile
	@mkdir -p $(BUILDDIR)
	sed "s/@LV2NAME@/$(LV2NAME)/g" \
	    lv2ttl/manifest.ttl.in > $(BUILDDIR)manifest.ttl
ifneq ($(BUILDOPENGL), no)
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@URI_SUFFIX@//g" \
	    lv2ttl/manifest.lv2.ttl.in >> $(BUILDDIR)manifest.ttl
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@UI_TYPE@/$(UI_TYPE)/;s/@LV2GUI1@/meters_glui/g;s/@LV2GUI2@/meters_glui/g;s/@LV2GUI3@/meters_glui/g;s/@LV2GUI4@/meters_glui/g;s/@LV2GUI5@/meters_glui/g;s/@LV2GUI6@/meters_glui/g;s/@LV2GUI7@/meters_glui/g;s/@LV2GUI8@/meters_glui/g;s/@LV2GUI9@/meters_glui/g" \
	    lv2ttl/manifest.gui.ttl.in >> $(BUILDDIR)manifest.ttl
endif
ifneq ($(BUILDGTK), no)
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@URI_SUFFIX@/_gtk/g" \
	    lv2ttl/manifest.lv2.ttl.in >> $(BUILDDIR)manifest.ttl
	sed "s/@LV2NAME@/$(LV2NAME)/g;s/@LIB_EXT@/$(LIB_EXT)/g;s/@LV2GTK1@/$(LV2GTK1)/g;s/@LV2GTK2@/$(LV2GTK2)/g;s/@LV2GTK3@/$(LV2GTK3)/g;s/@LV2GTK4@/$(LV2GTK4)/g;s/@LV2GTK5@/$(LV2GTK5)/g;s/@LV2GTK6@/$(LV2GTK6)/g;s/@LV2GTK7@/$(LV2GTK7)/g;s/@LV2GTK8@/$(LV2GTK8)/g;s/@LV2GTK9@/$(LV2GTK9)/g" \
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
	sed "s/@URI_SUFFIX@//g;s/@NAME_SUFFIX@//g;s/@DPMGUI@/$(DPMGUI)_gl/g;s/@EBUGUI@/$(EBUGUI)_gl/g;s/@GONGUI@/$(GONGUI)_gl/g;s/@MTRGUI@/$(MTRGUI)_gl/g;s/@KMRGUI@/$(KMRGUI)_gl/g;s/@MPWGUI@/$(MPWGUI)_gl/g;s/@SFSGUI@/$(SFSGUI)_gl/g;s/@DRMGUI@/$(DRMGUI)_gl/g;s/@SDHGUI@/$(SDHGUI)_gl/g" \
	  lv2ttl/$(LV2NAME).lv2.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
endif
ifneq ($(BUILDGTK), no)
	sed "s/@URI_SUFFIX@/_gtk/g;s/@NAME_SUFFIX@/ GTK/g;s/@DPMGUI@/$(DPMGUI)_gtk/g;s/@EBUGUI@/$(EBUGUI)_gtk/g;s/@GONGUI@/$(GONGUI)_gtk/g;s/@MTRGUI@/$(MTRGUI)_gtk/g;s/@KMRGUI@/$(KMRGUI)_gtk/g;s/@MPWGUI@/$(MPWGUI)_gtk/g;s/@SFSGUI@/$(SFSGUI)_gtk/g;s/@DRMGUI@/$(DRMGUI)_gtk/g;s/@SDHGUI@/$(SDHGUI)_gtk/g" \
	  lv2ttl/$(LV2NAME).lv2.ttl.in >> $(BUILDDIR)$(LV2NAME).ttl
endif

$(BUILDDIR)$(LV2NAME)$(LIB_EXT): src/meters.cc $(DSPDEPS) src/ebulv2.cc src/uris.h src/goniometerlv2.c src/goniometer.h src/spectrumlv2.c src/spectr.c src/xfer.c src/dr14.c src/sigdistlv2.c Makefile
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) \
	  -o $(BUILDDIR)$(LV2NAME)$(LIB_EXT) src/$(LV2NAME).cc $(DSPSRC) \
	  -shared $(LV2LDFLAGS) $(LDFLAGS) $(LOADLIBES)
	$(STRIP) $(STRIPFLAGS) $(BUILDDIR)$(LV2NAME)$(LIB_EXT)


JACKCFLAGS=-I. $(CFLAGS) $(CXXFLAGS) $(LIC_CFLAGS)
JACKCFLAGS+=`pkg-config --cflags jack lv2 pango pangocairo $(PKG_GL_LIBS)`
JACKLIBS=-lm $(GLUILIBS) $(LIC_LOADLIBES)

## JACK applications

$(eval x42_ebur128_JACKSRC = src/meters.cc $(DSPSRC))
x42_ebur128_JACKGUI = gui/ebur.c
x42_ebur128_LV2HTTL = lv2ttl/ebur128.h
x42_ebur128_JACKDESC = lv2ui_ebur
$(APPBLD)x42-ebur128$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_ebur128_JACKGUI) $(x42_ebur128_LV2HTTL)

$(eval x42_phase_correlation_JACKSRC = src/meters.cc $(DSPSRC))
x42_phase_correlation_JACKGUI = gui/needle.c
x42_phase_correlation_LV2HTTL = lv2ttl/cor.h
x42_phase_correlation_JACKDESC = lv2ui_cor
$(APPBLD)x42-phase-correlation$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_phase_correlation_JACKGUI) $(x42_phase_correlation_LV2HTTL)

$(eval x42_dr14_JACKSRC = src/meters.cc $(DSPSRC))
x42_dr14_JACKGUI = gui/dr14meter.c
x42_dr14_LV2HTTL = lv2ttl/dr14stereo.h
x42_dr14_JACKDESC = lv2ui_dr14
$(APPBLD)x42-dr14$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_dr14_JACKGUI) $(x42_dr14_LV2HTTL)

$(eval x42_k20rms_JACKSRC = src/meters.cc $(DSPSRC))
x42_k20rms_JACKGUI = gui/kmeter.c
x42_k20rms_LV2HTTL = lv2ttl/k20stereo.h
x42_k20rms_JACKDESC = lv2ui_k20stereo
$(APPBLD)x42-k20rms$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_k20rms_JACKGUI) $(x42_k20rms_LV2HTTL)

$(eval x42_goniometer_JACKSRC = src/meters.cc $(DSPSRC))
x42_goniometer_JACKGUI = gui/goniometer.c
x42_goniometer_LV2HTTL = lv2ttl/goniometer.h
x42_goniometer_JACKDESC = lv2ui_goniometer
$(APPBLD)x42-goniometer$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_goniometer_JACKGUI) $(x42_goniometer_LV2HTTL)

$(eval x42_phasewheel_JACKSRC = src/meters.cc $(DSPSRC) $(value FFTW))
x42_phasewheel_JACKGUI = gui/phasewheel.c
x42_phasewheel_LV2HTTL = lv2ttl/phasewheel.h
x42_phasewheel_JACKDESC = lv2ui_phasewheel
$(APPBLD)x42-phasewheel$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_phasewheel_JACKGUI) $(x42_phasewheel_LV2HTTL)

$(eval x42_histogram_JACKSRC = src/meters.cc $(DSPSRC))
x42_histogram_JACKGUI = gui/sdhmeter.c
x42_histogram_LV2HTTL = lv2ttl/sigdisthist.h
x42_histogram_JACKDESC = lv2ui_sigdisthist
$(APPBLD)x42-histogram$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_histogram_JACKGUI) $(x42_histogram_LV2HTTL)

$(eval x42_spectrum30_JACKSRC = src/meters.cc $(DSPSRC))
x42_spectrum30_JACKGUI = gui/dpm.c
x42_spectrum30_LV2HTTL = lv2ttl/spectr30.h
x42_spectrum30_JACKDESC = lv2ui_spectr30
$(APPBLD)x42-spectrum30$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_spectrum30_JACKGUI) $(x42_spectrum30_LV2HTTL)

$(eval x42_stereoscope_JACKSRC = src/meters.cc $(DSPSRC) $(value FFTW))
x42_stereoscope_JACKGUI = gui/stereoscope.c
x42_stereoscope_LV2HTTL = lv2ttl/stereoscope.h
x42_stereoscope_JACKDESC = lv2ui_stereoscope
$(APPBLD)x42-stereoscope$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_stereoscope_JACKGUI) $(x42_stereoscope_LV2HTTL)

$(eval x42_truepeakrms_JACKSRC = src/meters.cc $(DSPSRC))
x42_truepeakrms_JACKGUI = gui/dr14meter.c
x42_truepeakrms_LV2HTTL = lv2ttl/tp_rms_stereo.h
x42_truepeakrms_JACKDESC = lv2ui_tprms2
$(APPBLD)x42-truepeakrms$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) \
	$(x42_truepeakrms_JACKGUI) $(x42_truepeakrms_LV2HTTL)

gl_kmeter_LV2DESC = lv2ui_kmeter
gl_needle_LV2DESC = lv2ui_needle
gl_phasewheel_LV2DESC = lv2ui_phasewheel
gl_sdhmeter_LV2DESC = lv2ui_sdhmeter
gl_goniometer_LV2DESC = lv2ui_goniometer
gl_dr14meter_LV2DESC = lv2ui_dr14meter
gl_stereoscope_LV2DESC = lv2ui_stereoscope
gl_ebur_LV2DESC = lv2ui_ebur
gl_dpm_LV2DESC = lv2ui_dpm

COLLECTION_OBJS = \
	$(APPBLD)x42-ebur128.o \
	$(APPBLD)x42-phase-correlation.o \
	$(APPBLD)x42-dr14.o \
	$(APPBLD)x42-k20rms.o \
	$(APPBLD)x42-goniometer.o \
	$(APPBLD)x42-phasewheel.o \
	$(APPBLD)x42-histogram.o \
	$(APPBLD)x42-spectrum30.o \
	$(APPBLD)x42-stereoscope.o \
	$(APPBLD)x42-truepeakrms.o

$(eval x42_meter_collection_JACKSRC = -DX42_MULTIPLUGIN src/meters.cc $(DSPSRC) $(COLLECTION_OBJS) $(value FFTW))
x42_meter_collection_LV2HTTL = lv2ttl/plugins.h
$(APPBLD)x42-meter-collection$(EXE_EXT): src/meters.cc $(DSPSRC) $(DSPDEPS) $(COLLECTION_OBJS) \
	lv2ttl/cor.h lv2ttl/dr14stereo.h lv2ttl/ebur128.h lv2ttl/goniometer.h \
	lv2ttl/k20stereo.h lv2ttl/phasewheel.h lv2ttl/sigdisthist.h lv2ttl/spectr30.h \
	lv2ttl/stereoscope.h lv2ttl/tp_rms_stereo.h lv2ttl/plugins.h


-include $(RW)robtk.mk

$(BUILDDIR)$(LV2GTK1)$(LIB_EXT): $(UIIMGS) src/uris.h gui/needle.c gui/meterimage.c
$(BUILDDIR)$(LV2GTK2)$(LIB_EXT): gui/ebur.c src/uris.h
$(BUILDDIR)$(LV2GTK3)$(LIB_EXT): gui/goniometer.c src/goniometer.h \
    $(goniometer_UIDEP) zita-resampler/resampler.h zita-resampler/resampler-table.h
$(BUILDDIR)$(LV2GTK4)$(LIB_EXT): gui/dpm.c
$(BUILDDIR)$(LV2GTK5)$(LIB_EXT): gui/kmeter.c
$(BUILDDIR)$(LV2GTK6)$(LIB_EXT): gui/phasewheel.c src/uri2.h gui/fft.c
$(BUILDDIR)$(LV2GTK7)$(LIB_EXT): gui/stereoscope.c src/uri2.h gui/fft.c
$(BUILDDIR)$(LV2GTK8)$(LIB_EXT): gui/dr14meter.c
$(BUILDDIR)$(LV2GTK9)$(LIB_EXT): gui/sdhmeter.c

$(OBJDIR)$(LV2GUI1).o: $(UIIMGS) src/uris.h gui/needle.c gui/meterimage.c
$(OBJDIR)$(LV2GUI2).o: gui/ebur.c src/uris.h
$(OBJDIR)$(LV2GUI3).o: gui/goniometer.c src/goniometer.h \
    $(goniometer_UIDEP) zita-resampler/resampler.h zita-resampler/resampler-table.h
$(OBJDIR)$(LV2GUI4).o: gui/dpm.c
$(OBJDIR)$(LV2GUI5).o: gui/kmeter.c
$(OBJDIR)$(LV2GUI6).o: gui/phasewheel.c src/uri2.h gui/fft.c
$(OBJDIR)$(LV2GUI7).o: gui/stereoscope.c src/uri2.h gui/fft.c
$(OBJDIR)$(LV2GUI8).o: gui/dr14meter.c
$(OBJDIR)$(LV2GUI9).o: gui/sdhmeter.c

GLGUIOBJ = $(OBJDIR)pugl.o \
					 $(OBJDIR)$(LV2GUI1).o \
					 $(OBJDIR)$(LV2GUI2).o \
					 $(OBJDIR)$(LV2GUI3).o \
					 $(OBJDIR)$(LV2GUI4).o \
					 $(OBJDIR)$(LV2GUI5).o \
					 $(OBJDIR)$(LV2GUI6).o \
					 $(OBJDIR)$(LV2GUI7).o \
					 $(OBJDIR)$(LV2GUI8).o \
					 $(OBJDIR)$(LV2GUI9).o

$(BUILDDIR)meters_glui.so: gui/meters.c $(GLGUIOBJ) $(goniometer_UIDEP)

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
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/meters_glui$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK1)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK2)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK3)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK4)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK5)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK6)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK7)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK8)$(LIB_EXT)
	rm -f $(DESTDIR)$(LV2DIR)/$(BUNDLE)/$(LV2GTK9)$(LIB_EXT)
	-rmdir $(DESTDIR)$(LV2DIR)/$(BUNDLE)

clean:
	rm -f $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl \
	  $(BUILDDIR)$(LV2NAME)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK1)$(LIB_EXT) $(BUILDDIR)$(LV2GTK2)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK3)$(LIB_EXT) $(BUILDDIR)$(LV2GTK4)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK5)$(LIB_EXT) $(BUILDDIR)$(LV2GTK6)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK7)$(LIB_EXT) $(BUILDDIR)$(LV2GTK8)$(LIB_EXT) \
	  $(BUILDDIR)$(LV2GTK9)$(LIB_EXT) $(BUILDDIR)meters_glui$(LIB_EXT)
	rm -f $(OBJDIR)pugl.o \
	  $(OBJDIR)$(LV2GUI1).o $(OBJDIR)$(LV2GUI2).o \
	  $(OBJDIR)$(LV2GUI3).o $(OBJDIR)$(LV2GUI4).o \
	  $(OBJDIR)$(LV2GUI5).o $(OBJDIR)$(LV2GUI6).o \
	  $(OBJDIR)$(LV2GUI7).o $(OBJDIR)$(LV2GUI8).o \
	  $(OBJDIR)$(LV2GUI9).o
	rm -rf $(BUILDDIR)*.dSYM
	rm -rf $(APPBLD)x42-*
	-test -d $(APPBLD) && rmdir $(APPBLD) || true
	-test -d $(BUILDDIR) && rmdir $(BUILDDIR) || true
	-test -d $(OBJDIR) && rmdir $(OBJDIR) || true

distclean: clean
	rm -f cscope.out cscope.files tags

.PHONY: clean all install uninstall distclean jackapps \
        submodule_check submodules submodule_update submodule_pull
