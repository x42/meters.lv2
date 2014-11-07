#!/bin/bash

pushd "`/usr/bin/dirname \"$0\"`" > /dev/null; this_script_dir="`pwd`"; popd > /dev/null
cd $this_script_dir/..

: ${XARCH=i686} # or x86_64
: ${TMPDIR=/var/tmp}

if test "$XARCH" = "x86_64" -o "$XARCH" = "amd64"; then
	echo "Target: 64bit Windows (x86_64)"
	XPREFIX=x86_64-w64-mingw32
	WARCH=w64
else
	echo "Target: 32 Windows (i686)"
	XPREFIX=i686-w64-mingw32
	WARCH=w32
fi

if [ "$(id -u)" = "0" ]; then
	apt-get -y install nsis curl
fi

################################################################################
set -e

GITVERSION=$(git describe --tags | sed 's/-g.*$//')
BUILDDATE=$(date -R)
BINVERSION=$(git describe --tags | sed 's/-g.*$//' | sed 's/-/./')

if ! test -f x42/x42-meter-collection.exe -o ! -f build/meters.dll; then
	echo "*** No binaries found."
	exit 1
fi

OUTFILE="${TMPDIR}/x42-meters-${GITVERSION}-${WARCH}-Setup.exe"

################################################################################

if test -z "$DESTDIR"; then
	DESTDIR=`mktemp -d`
	trap 'rm -rf $DESTDIR' exit SIGINT SIGTERM
fi

echo " === bundle to $DESTDIR"

mkdir -p $DESTDIR/share

cp -a build $DESTDIR/meters.lv2
cp x42/x42-meter-collection.exe $DESTDIR
cp robtk/COPYING $DESTDIR/share/
cp img/x42.ico $DESTDIR/share/
cp tools/win_readme.txt $DESTDIR/README.txt

echo " === complete"
du -sh $DESTDIR

################################################################################
echo " === Preparing Windows Installer"
NSISFILE=$DESTDIR/x42.nsis

if test "$WARCH" = "w64"; then
	PGF=PROGRAMFILES64
	CMF=COMMONFILES64
else
	PGF=PROGRAMFILES
	CMF=COMMONFILES
fi

if test -n "$QUICKZIP" ; then
	cat > $NSISFILE << EOF
SetCompressor zlib
EOF
else
	cat > $NSISFILE << EOF
SetCompressor /SOLID lzma
SetCompressorDictSize 32
EOF
fi

cat >> $NSISFILE << EOF
!include MUI2.nsh
Name "x42-Meters"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
InstallDir "\$${PGF}\\x42-meters"
InstallDirRegKey HKLM "Software\\RSS\\x42meters\\$WARCH" "Install_Dir"

!define MUI_ICON "share\\x42.ico"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "share\\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Meters.lv2 (required)" SecLV2
  SectionIn RO
  SetOutPath "\$${CMF}\\LV2"
  File /r meters.lv2
  SetOutPath "\$INSTDIR"
  File README.txt
  WriteRegStr HKLM SOFTWARE\\RSS\\x42meters\\$WARCH "Install_Dir" "\$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\x42meters" "DisplayName" "x42-meters"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\x42meters" "UninstallString" '"\$INSTDIR\\uninstall.exe"'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\x42meters" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\x42meters" "NoRepair" 1
  WriteUninstaller "\$INSTDIR\uninstall.exe"
  SetShellVarContext all
  CreateDirectory "\$SMPROGRAMS\\x42-meters"
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe" "" "\$INSTDIR\\uninstall.exe" 0
SectionEnd

Section "JACK Application" SecJACK
  SetOutPath \$INSTDIR
  File x42-meter-collection.exe
  SetShellVarContext all
  CreateDirectory "\$SMPROGRAMS\\x42-meters"
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Phase Correlation.lnk" "\$INSTDIR\\x42-meter-collection.exe" "0" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\DR14 - Dynamic Range.lnk" "\$INSTDIR\\x42-meter-collection.exe" "1" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\EBUr128.lnk" "\$INSTDIR\\x42-meter-collection.exe" "2" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Goniometer.lnk" "\$INSTDIR\\x42-meter-collection.exe" "3" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\K20 RMS.lnk" "\$INSTDIR\\x42-meter-collection.exe" "4" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Phasewheel.lnk" "\$INSTDIR\\x42-meter-collection.exe" "5" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Signal Distribution Histogram.lnk" "\$INSTDIR\\x42-meter-collection.exe" "6" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\30 Band Spectrum Analyzer.lnk" "\$INSTDIR\\x42-meter-collection.exe" "7" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\Stereo vs Frequency Scope.lnk" "\$INSTDIR\\x42-meter-collection.exe" "8" "\$INSTDIR\\x42-meter-collection.exe" 0
  CreateShortCut "\$SMPROGRAMS\\x42-meters\\True Peak and RMS.lnk" "\$INSTDIR\\x42-meter-collection.exe" "9" "\$INSTDIR\\x42-meter-collection.exe" 0
SectionEnd

LangString DESC_SecLV2 \${LANG_ENGLISH} "x42-meters.lv2 ${GITVERSION}\$\\r\$\\nLV2 Plugins.\$\\r\$\\n${BUILDDATE}"
LangString DESC_SecJACK \${LANG_ENGLISH} "Standalone JACK clients and start-menu shortcuts"
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT \${SecLV2} \$(DESC_SecLV2)
!insertmacro MUI_DESCRIPTION_TEXT \${SecJACK} \$(DESC_SecJACK)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  SetShellVarContext all
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\x42meters"
  DeleteRegKey HKLM SOFTWARE\\RSS\\x42meters\\$WARCH
  RMDir /r "\$${CMF}\\LV2\\meters.lv2"
  Delete "\$INSTDIR\\x42-meter-collection.exe"
  Delete "\$INSTDIR\\README.txt"
	Delete "\$INSTDIR\uninstall.exe"
  RMDir "\$INSTDIR"
  Delete "\$SMPROGRAMS\\x42-meters\\*.*"
  RMDir "\$SMPROGRAMS\\x42-meters"
SectionEnd
EOF

#cat -n $NSISFILE

rm -f ${OUTFILE}
echo " === OutFile: $OUTFILE"

if test -n "$QUICKZIP" ; then
echo " === Building Windows Installer (fast zip)"
else
echo " === Building Windows Installer (lzma compression takes ages)"
fi
time makensis -V2 $NSISFILE
rm -rf $DESTDIR
ls -lh "$OUTFILE"
