#!/usr/bin/bash
###########################################################################
# This file is a reminder of how I built wxWidgets 3.1.3 on Windows.
#
# Use the script like:
#
#   bash
#   cd [wxwidgets_src_dir]
#   patch -p1 < [path_to_this_script]/wxwidgets-3.1.3.patch
#
#   cd [wxwidgets_bin_dir]/retail
#   [path_to_this_script]/configure-wxwidgets.sh i686 [wxwidgets_src_dir]
#   make clean all
#
#   cd [wxwidgets_bin_dir]/retail64
#   [path_to_this_script]/configure-wxwidgets.sh x86_64 [wxwidgets_src_dir]
#   make clean all
#
#   export DEBUG=1
#   cd [wxwidgets_bin_dir]/debug
#   [path_to_this_script]/configure-wxwidgets.sh i686 [wxwidgets_src_dir]
#   make clean all
#
#   cd [wxwidgets_bin_dir]/debug64
#   [path_to_this_script]/configure-wxwidgets.sh x86_64 [wxwidgets_src_dir]
#   make clean all
#
###########################################################################
if [ -z ${1+x} ];
then
  echo "Usage: configure-wxwidgets.sh i686|x86_64 <path to wxwidgets src>"
  exit 1
fi

ARCH=$1
CONFIGURE=$2/configure

if [ ! -f $CONFIGURE ];
then
  echo "The file $CONFIGURE does not exist"
  exit 1
fi

if [ -f configarg.cache ];
then
  rm configarg.cache
fi

if [ "$DEBUG" == "1" ];
then
  export ADDITIONAL_OPTIONS="--enable-debug --enable-debug_gdb --enable-debug_info --disable-optimise"
else
  export ADDITIONAL_OPTIONS="--disable-debug_flag --enable-optimise"

  # Enable link-time optimization so that the resulting fmslogo.exe isn't 8 MiB.
  # Without this, all of the uncalled member functions remain in the final executable.
  ## Unfortunately, I cannot figure out how to compile FMSLogo without getting
  ## multiple definition errors for wxTextCtrlBase::SetValue(wxString const&) and
  ## wxTextCtrlBase::GetValue().  As a result, I cannot compile FMSLogo in a way
  ## which removes the dead code.
  ##export AR=gcc-ar
  ##export RANLIB=gcc-ranlib
  ##export CFLAGS=-flto
  ##export CXXFLAGS=-flto
  ##export LDFLAGS=-flto
fi

if [ "$PROFILE" == "1" ];
then
export ADDITIONAL_OPTIONS="$ADDITIONAL_OPTIONS --enable-profile"
fi

# Statically link to libc.  This has no impact on FMSLogo, as we only build a static
# library for FMSLogo and the linker is not involved in that.  However, this makes
# it easier to run the wxWidgets sample applications.
export LDFLAGS="-static-libgcc -static-libstdc++ -static $LDFLAGS"

# Ensure that UNIX tools like /bin/find are used instead of the Windows tools
# with the same name.
export PATH=/usr/bin:$PATH

# Prevent the builtin expat library from complaining that it cannot build the
# man pages.  This doesn't enable DOCBOOK_TO_MAN, it makes it a no-op by using
# the true command instead of something which converts docbook to man pages.
export DOCBOOK_TO_MAN=true

$CONFIGURE                        \
    $ADDITIONAL_OPTIONS           \
    --host=$ARCH-w64-mingw32      \
    --disable-all-features        \
    --disable-metafile            \
    --disable-postscript          \
    --disable-largefile           \
    --disable-compat30            \
    --disable-gtktest             \
    --disable-sdltest             \
    --disable-dependency-tracking \
    --with-msw                    \
    --with-expat=builtin          \
    --with-regex=no               \
    --with-libpng=no              \
    --with-zlib=no                \
    --with-libjpeg=builtin        \
    --with-libtiff=no             \
    --with-opengl=no              \
    --with-libnotify=no           \
    --enable-unicode              \
    --enable-monolithic           \
    --enable-pch                  \
    --disable-shared              \
    --enable-stc                  \
    --enable-snglinst             \
    --enable-stopwatch            \
    --enable-longlong             \
    --enable-menus                \
    --enable-notifmsg             \
    --enable-timer                \
    --enable-button               \
    --enable-stattext             \
    --enable-image                \
    --enable-bmpbutton            \
    --enable-statbmp              \
    --enable-textctrl             \
    --enable-scrollbar            \
    --enable-streams              \
    --enable-clipboard            \
    --enable-file                 \
    --enable-ffile                \
    --enable-dataobj              \
    --enable-ole                  \
    --enable-variant              \
    --enable-listctrl             \
    --enable-listbox              \
    --enable-imaglist             \
    --enable-richtext             \
    --enable-html                 \
    --enable-filesystem           \
    --enable-printarch            \
    --enable-combobox             \
    --enable-choice               \
    --enable-msgdlg               \
    --enable-fontmap              \
    --enable-finddlg              \
    --enable-togglebtn            \
    --enable-mshtmlhelp           \
    --enable-help                 \
    --enable-textbuf              \
    --enable-textfile             \
    --enable-choicedlg            \
    --enable-dynamicloader        \
    --enable-dynlib               \
    --enable-fontdlg              \
    --enable-fontenum             \
    --enable-propgrid             \
    --enable-coldlg               \
    --enable-odcombobox           \
    --enable-comboctrl            \
    --enable-editablebox          \
    --enable-regkey               \
    --enable-filedlg              \
    --enable-dirdlg               \
    --enable-treectrl             \
    --enable-notebook             \
    --enable-caret                \
    --enable-accel                \
    --enable-spinbtn              \
    --enable-checkbox             \
    --enable-radiobtn             \
    --enable-statline             \
    --enable-spinctrl             \
    --enable-textdlg              \
    --enable-statbox              \
    --enable-stdpaths             \
    --enable-slider               \
    --enable-splitter             \
    --enable-uxtheme              \
    --enable-dccache              \
    --enable-wxdib                \
    --enable-mousewheel           \
    --enable-popupwin             \
    --enable-display              \
    --enable-hyperlink            \
    --enable-sound                \
