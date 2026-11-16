# =========================================================================
# NOTE: this makefile is a stop-gap solution for me (david costanzo) to
# to build the FMSlogo on my machines.  I don't expect this to work for
# anyone else without modification.
# =========================================================================
include ../version.mk
include ../lcids.mk

ifeq "$(shell uname)" "Linux"

EXEEXT =

TOOLKIT = GTK

WXCONFIG = wx-config

wx_top_builddir = $(shell $(WXCONFIG) --prefix)

ifeq "$(DEBUG)" "1"
LDFLAGS  = $(shell $(WXCONFIG) --debug=yes --libs base,core,stc,richtext,html,adv)
TOOLCHAIN_FULLNAME = gtk2-unicode-debug-$(shell $(WXCONFIG) --release)
else
LDFLAGS  = $(shell $(WXCONFIG) --debug=no --libs base,core,stc,richtext,html,adv)
TOOLCHAIN_FULLNAME = gtk2-unicode-release-$(shell $(WXCONFIG) --release)
endif

CPPFLAGS += $(shell $(WXCONFIG) --cppflags) -DWX_PURE
CXXFLAGS += -pthread -Wno-unused-variable -Wno-unused-local-typedefs -Wno-unused-function -Wno-unused-but-set-variable
LDFLAGS  += -pthread

SCINTILLA_INCLUDES = scintilla/include

else

# assume this is my Windows box

EXEEXT = .exe

wx_top_srcdir   = /cygdrive/e/wxWidgets-3.1.3-src

ifeq "$(DEBUG)" "1"
wx_top_builddir = /cygdrive/e/wxWidgets-3.1.3-fmslogo/debug
else
wx_top_builddir = /cygdrive/e/wxWidgets-3.1.3-fmslogo/retail
endif

# Compile with MinGW
TARGET_PREFIX      = i686-w64-mingw32
TOOLCHAIN_FULLNAME = $(TARGET_PREFIX)-msw-unicode-static-3.1
RC = $(TARGET_PREFIX)-windres --preprocessor=$(CXX) --preprocessor-arg=-E --preprocessor-arg=-xc-header

LDFLAGS = $(addprefix -l,$(addsuffix -3.1-$(TARGET_PREFIX),wx_mswu wxscintilla wxjpeg wxexpat))
LDFLAGS += -mwindows

TOOLKIT = MSW

EXTRALIBS = -static-libgcc -static-libstdc++ -static -lrpcrt4 -loleaut32 -lole32 -luuid -lwinspool -lwinmm -lshell32 -lshlwapi -lcomctl32 -lcomdlg32 -ladvapi32 -lversion -lwsock32 -lgdi32 -luxtheme -loleacc

SCINTILLA_INCLUDES = $(wx_top_srcdir)/src/stc/scintilla/include $(wx_top_srcdir)/src/stc/scintilla/lexlib $(wx_top_srcdir)/src/stc/scintilla/src

# 64-bit build
wx_top_builddir64    = $(wx_top_builddir)64
TARGET_PREFIX64      = x86_64-w64-mingw32
TOOLCHAIN_FULLNAME64 = $(TARGET_PREFIX64)-msw-unicode-static-3.1
RC64                 = $(TARGET_PREFIX64)-windres --preprocessor=$(CXX64) --preprocessor-arg=-E --preprocessor-arg=-xc-header
LDFLAGS64            = $(addprefix -l,$(addsuffix -3.1-$(TARGET_PREFIX64),wx_mswu wxscintilla wxjpeg wxexpat)) -mwindows

endif


ifeq "$(DEBUG)" "1"
CPPFLAGS += -D__WXDEBUG__ -DMEM_DEBUG -DDEBUG
CXXFLAGS += -O0 -g -ggdb
else
CPPFLAGS += -DNDEBUG -DwxDEBUG_LEVEL=0
CXXFLAGS += -O2

ifneq "$(PROFILE)"  "1"
ifneq "$(COVERAGE)" "1"
# On retail builds, strip symbols unless we need them for profiling.
# CONSIDER: using a separate variable for offical releases to strip. 
LDFLAGS  += -s
endif
endif

endif

# Setup the build for profiling, if requested.
ifeq "$(PROFILE)" "1"
CFLAGS   += -pg -g
CXXFLAGS += -pg -g
LDFLAGS  += -pg -g
endif

# Setup the build for code coverage, if requested.
ifeq "$(COVERAGE)" "1"
CFLAGS   += --coverage
CXXFLAGS += --coverage
LDFLAGS  += --coverage
endif

INCLUDES   = . ../../native/include $(wx_top_builddir)/lib/wx/include/$(TOOLCHAIN_FULLNAME) $(wx_top_srcdir)/include
INCLUDES64 = . ../../native/include $(wx_top_builddir64)/lib/wx/include/$(TOOLCHAIN_FULLNAME64) $(wx_top_srcdir)/include
CPPFLAGS += -DUNICODE -D_UNICODE -DUSE_PRECOMPILED_HEADER -Winvalid-pch
CXXFLAGS += -Wall -fno-strict-aliasing
CXXFLAGS += -std=c++17
CXXFLAGS += -DFMSLOGO_WXWIDGETS

#BK_DEPS = $(wx_top_builddir)/bk-deps
LIBS = -lm
CXX   = $(TARGET_PREFIX)-g++
CXX64 = $(TARGET_PREFIX64)-g++

LIBDIRNAME   = $(wx_top_builddir)/lib
LIBDIRNAME64 = $(wx_top_builddir64)/lib
CXXFLAGS32   = $(CXXFLAGS) -DFMSLOGO_WORDBITS=32 -D__WX$(TOOLKIT)__ $(CPPFLAGS) $(addprefix -I, $(INCLUDES))
CXXFLAGS64   = $(CXXFLAGS) -DFMSLOGO_WORDBITS=64 -D__WX$(TOOLKIT)__ $(CPPFLAGS) $(addprefix -I, $(INCLUDES64))

MSW_OBJECTS =                  \
    LexFmsLogo.o               \
    LexFmsLogoRegisterer.o     \
    netwind.o                  \

GTK_OBJECTS =                  \


WX_OBJECTS =                   \
    3dsolid.o                  \
    aboutfmslogo.o             \
    aboutmultiplesclerosis.o   \
    activearea.o               \
    appendablelist.o           \
    argumentutils.o            \
    avltree.o                  \
    centerablelink.o           \
    commander.o                \
    commanderbutton.o          \
    commanderhistory.o         \
    commanderinput.o           \
    commandertogglebutton.o    \
    coms.o                     \
    cursor.o                   \
    devwind.o                  \
    dynamicbuffer.o            \
    editproceduredialog.o      \
    eraseproceduredialog.o     \
    error.o                    \
    eval.o                     \
    files.o                    \
    fileswnd.o                 \
    fmslogo.o                  \
    fontutils.o                \
    gbm.o                      \
    gbmbmp.o                   \
    gbmgif.o                   \
    gbmhelp.o                  \
    gbmsize.o                  \
    graphics.o                 \
    graphwin.o                 \
    guiutils.o                 \
    helputils.o                \
    ibmterm.o                  \
    init.o                     \
    intern.o                   \
    legacyansi.o               \
    lists.o                    \
    localizednode.o            \
    logocodectrl.o             \
    logodata.o                 \
    logodialogboxes.o          \
    logoeventqueue.o           \
    logomath.o                 \
    mainframe.o                \
    mainwind.o                 \
    mem.o                      \
    messagebox.o               \
    musiccommands.o            \
    music_api.o                \
    music_engine.o             \
    minieditor.o               \
    minieditortextctrl.o       \
    paren.o                    \
    parse.o                    \
    pausedialog.o              \
    print.o                    \
    questionbox.o              \
    savebeforeexitdialog.o     \
    screen.o                   \
    selectbox.o                \
    selectproceduredialog.o    \
    selectstartupinstruction.o \
    setactivearea.o            \
    setcolor.o                 \
    setpensize.o               \
    sort.o                     \
    startup.o                  \
    statusdialog.o             \
    stringprintednode.o        \
    term.o                     \
    threed.o                   \
    timer.o                    \
    unix.o                     \
    utils.o                    \
    unicodefile.o              \
    vector.o                   \
    workspaceeditor.o          \
    wrksp.o                    \
    debugheap.o                \
    debugutils.o               \


ifeq "$(TOOLKIT)" "MSW"
OBJECTS = $(sort $(MSW_OBJECTS) $(WX_OBJECTS))
else
OBJECTS = $(sort $(GTK_OBJECTS) $(WX_OBJECTS)) 
endif

#CXXC = $(CXX)
CXXC = $(BK_DEPS) $(CXX)

### Targets: ###

# Set the English version as the first target, which will get
# built if no target is provided to make.
default : fmslogo$(EXEEXT) logohelp.chm

fmslogo$(EXEEXT) : fmslogo-$(DEFAULT_LCID)$(EXEEXT)
	cp $< $@

fmslogo64$(EXEEXT) : fmslogo64-$(DEFAULT_LCID)$(EXEEXT)
	cp $< $@

logo-midi64$(EXEEXT) : logo-midi-$(DEFAULT_LCID)$(EXEEXT)
	cp $< $@

logohelp.chm : ../manual/logohelp-1033.chm
	cp $< $@

# The "all" target is used to build all locales.
all : $(foreach lcid,$(LCIDS),fmslogo-${lcid}$(EXEEXT) fmslogo64-${lcid}$(EXEEXT))
	perl -W make-translation-tables.pl

clean: 
	rm -rf ./.deps
	$(RM) ./obj/*
	$(RM) ./obj64/*
	$(RM) ./*.bak
	$(RM) ./*~
	$(RM) ./*.stackdump
	$(RM) $(foreach lcid,$(LCIDS),fmslogo-${lcid}$(EXEEXT))   fmslogo$(EXEEXT)
	$(RM) $(foreach lcid,$(LCIDS),fmslogo64-${lcid}$(EXEEXT)) fmslogo64$(EXEEXT)
	$(RM) $(foreach lcid,$(LCIDS),logo-midi-${lcid}$(EXEEXT)) logo-midi64$(EXEEXT)
	$(RM) logohelp.chm
	$(RM) $(wildcard version.h)
	$(RM) $(shell /usr/bin/find . -iname '*.obj')
	$(RM) $(shell /usr/bin/find . -iname '*.res')
	$(RM) $(shell /usr/bin/find . -iname '*.bak')
	$(RM) $(shell /usr/bin/find . -iname '*~')
	$(RM) $(shell /usr/bin/find . -iname '*.csm')
	$(RM) $(wildcard toenglish-*.txt)
	$(RM) $(wildcard fromenglish-*.txt)
	$(RM) $(wildcard translations-*.xml)
	$(RM) $(wildcard Examples/3d/3dmovie.gif)


#
# Rules to create version.h
#
version.h : ../version.mk
	echo \#define FMSLOGO_MAJOR_VERSION $(FMSLOGO_MAJOR_VERSION)  > $@
	echo \#define FMSLOGO_MINOR_VERSION $(FMSLOGO_MINOR_VERSION) >> $@
	echo \#define FMSLOGO_MICRO_VERSION $(FMSLOGO_MICRO_VERSION) >> $@
ifneq "$(FMSLOGO_VERSION_SUFFIX)" ""
	echo \#define FMSLOGO_PRIVATE_BUILD                          >> $@
endif
	echo \#define FMSLOGO_VERSION L\"$(FMSLOGO_VERSION)\"        >> $@


#
# Rules to build locale-independent parts of the main environment.
#
obj/pch.h.gch : pch/pch.h version.h *.h
	$(CXX) -c -o $@ -Iobj $(CXXFLAGS32) $<

obj/%.o : %.cpp version.h obj/pch.h.gch
	$(CXX) -c -o $@ -Iobj $(CXXFLAGS32) $<

obj/%.o : scintilla/%.cxx obj/pch.h.gch
	$(CXX) -c -o $@ -Iobj $(CXXFLAGS32) $(addprefix -I,$(SCINTILLA_INCLUDES)) $<

#
# 64-bit locale-independent parts of the main environment.
#
obj64/pch.h.gch : pch/pch.h version.h *.h
	$(CXX64) -c -o $@ -Iobj64 $(CXXFLAGS64) $<

obj64/%.o : %.cpp version.h obj64/pch.h.gch
	$(CXX64) -c -o $@ -Iobj64 $(CXXFLAGS64) $<

obj64/%.o : scintilla/%.cxx obj64/pch.h.gch
	$(CXX64) -c -o $@ -Iobj64 $(CXXFLAGS64) $(addprefix -I,$(SCINTILLA_INCLUDES)) $<

obj/music_api.o : ../../native/src/music_api.cpp ../../native/include/logo_midi/music_api.hpp
	$(CXX) -c -o $@ $(CXXFLAGS32) $<

obj64/music_api.o : ../../native/src/music_api.cpp ../../native/include/logo_midi/music_api.hpp
	$(CXX64) -c -o $@ $(CXXFLAGS64) $<

obj/music_engine.o : ../../native/src/music_engine.cpp ../../native/include/logo_midi/music_engine.hpp
	$(CXX) -c -o $@ $(CXXFLAGS32) $<

obj64/music_engine.o : ../../native/src/music_engine.cpp ../../native/include/logo_midi/music_engine.hpp
	$(CXX64) -c -o $@ $(CXXFLAGS64) $<

#
# Generate the rules to build the release version of the main environment for each locale
#
define FMSLOGO_template

ifeq "$(TOOLKIT)" "MSW"
# On Windows, compile with the RC file.
fmslogo-$(1)$$(EXEEXT) : $$(addprefix obj/,$$(OBJECTS) localizedstrings-$(1).o fmslogo_rc-$(1).o)
	$(CXX) -o $$@ $$^ $(LDFLAGS) -Wl,--large-address-aware -L$(LIBDIRNAME) $(LIBS) $(EXTRALIBS)

obj/%_rc-$(1).o : %.rc version.h obj/pch.h.gch fmslogo.ico fmslogo.exe.manifest
	$(RC) -i$$< -o$$@ -DLOCALE=$1 --define __WX$(TOOLKIT)__ $(addprefix --include-dir ,$(INCLUDES))

# Windows 64-bit build
fmslogo64-$(1)$$(EXEEXT) : $$(addprefix obj64/,$$(OBJECTS) localizedstrings-$(1).o fmslogo_rc-$(1).o)
	$(CXX64) -o $$@ $$^ $(LDFLAGS64) -L$(LIBDIRNAME64) $(LIBS) $(EXTRALIBS)

# Windows 64-bit command-line MIDI renderer.
logo-midi-$(1)$$(EXEEXT) : $$(addprefix obj64/,$$(OBJECTS) localizedstrings-$(1).o logo-midi_rc-$(1).o)
	$(CXX64) -o $$@ $$^ $(LDFLAGS64) -mconsole -L$(LIBDIRNAME64) $(LIBS) $(EXTRALIBS)

obj64/%_rc-$(1).o : %.rc version.h obj64/pch.h.gch fmslogo.ico fmslogo.exe.manifest
	$(RC64) -i$$< -o$$@ -DLOCALE=$1 --define __WX$(TOOLKIT)__ $(addprefix --include-dir ,$(INCLUDES64))


else
# On Linux, no RC file is used.
fmslogo-$(1)$$(EXEEXT) : $$(addprefix obj/,$$(OBJECTS) localizedstrings-$(1).o)
	$(CXX) -o $$@ $$^ $(LDFLAGS) -L$(LIBDIRNAME) $(LIBS) $(EXTRALIBS)
endif

obj/localizedstrings-$(1).o : localizedstrings.cpp localizedstrings.h
	$(CXX) -c -o $$@ -DLOCALE=$1 $(CXXFLAGS) $$<

obj64/localizedstrings-$(1).o : localizedstrings.cpp localizedstrings.h
	$(CXX64) -c -o $$@ -DLOCALE=$1 $(CXXFLAGS) $$<

endef

$(foreach lcid,$(LCIDS),$(eval $(call FMSLOGO_template,$(lcid))))

# Include dependency info, if present:
-include .deps/*.d

tags : *.cpp *.h
	etags localizedstrings-en.h $(shell /usr/bin/find . -iname '*.cpp' -o -iname '*.h' -o -iname '*.c' -o -iname '*.cxx')

screensaver :
	$(MAKE) -C ../screensaver

test :
	perl check-logo-code.pl logolib/*
	perl check-logo-code.pl Examples/3d/*.LGO
	perl check-logo-code.pl Examples/Misc/*.LGO
	perl check-logo-code.pl Examples/Multimed/*.LGO
	perl check-logo-code.pl Examples/Network/*.LGO
	perl check-logo-code.pl Examples/Pascal/*.LGO
	perl check-logo-code.pl Examples/UCBLogo/*.LGO
	perl check-logo-code.pl Examples/Windows/*.LGO

# gcov assumes that the source code and the object files are in the same directory.
# I expect there's a better way to do this.
coverage-reports:
	cp --preserve=timestamps *.cpp *.h obj
	-(cd obj && gcov --all-blocks --branch-counts *.cpp *.h)
	@echo
	@echo "Your code coverage reports are in obj/*.gcov"

.PHONY: all clean default test coverage-reports
