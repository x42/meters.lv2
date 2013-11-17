RT=$(RW)rtk/
WD=$(RW)widgets/robtk_

UITOOLKIT=$(WD)checkbutton.h $(WD)dial.h $(WD)label.h $(WD)pushbutton.h\
          $(WD)radiobutton.h $(WD)scale.h $(WD)separator.h $(WD)spinner.h \
          $(WD)xyplot.h $(WD)selector.h

ROBGL= $(RW)robtk.mk $(UITOOLKIT) $(RW)ui_gl.c $(PUGL_SRC) \
  $(RW)gl/common_cgl.h $(RW)gl/layout.h $(RW)gl/robwidget_gl.h $(RW)robtk.h \
	$(RT)common.h $(RT)style.h \
  $(RW)gl/xternalui.c $(RW)gl/xternalui.h

ROBGTK = $(RW)robtk.mk $(UITOOLKIT) $(RW)ui_gtk.c \
  $(RW)gtk2/common_cgtk.h $(RW)gtk2/robwidget_gtk.h $(RW)robtk.h \
	$(RT)common.h $(RT)style.h

%UI_gtk.so %UI_gtk.dylib:: $(ROBGTK)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(GTKUICFLAGS) \
	  -DPLUGIN_SOURCE="\"gui/$(*F).c\"" \
	  -o $@ $(RW)ui_gtk.c \
	  $(value $(*F)_UISRC) \
	  -shared $(LV2LDFLAGS) $(LDFLAGS) $(GTKUILIBS)

%UI_gl.so %UI_gl.dylib:: $(ROBGL)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(GLUICFLAGS) \
	  -DPLUGIN_SOURCE="\"gui/$(*F).c\"" \
	  -o $@ $(RW)ui_gl.c \
	  $(PUGL_SRC) \
	  $(value $(*F)_UISRC) \
	  -shared $(LV2LDFLAGS) $(LDFLAGS) $(GLUILIBS)

