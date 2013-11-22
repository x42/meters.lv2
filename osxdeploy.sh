#!/bin/sh

# note libpango needs to be configured using --without-x --with-included-modules=yes

TLD=$(pwd)

if test "$ARCH" = i386; then
  make clean all CFLAGS="-arch i386 -mmacosx-version-min=10.5" LDFLAGS="-arch i386 -mmacosx-version-min=10.5"
else
  ARCH=x86_64
  make clean all CFLAGS="-mmacosx-version-min=10.5" LDFLAGS="-mmacosx-version-min=10.5" KXURI=no
fi

export TARGET_BUILD_DIR="$TLD/build/"
export TARGET_DEPLOY_DIR="$TLD/meters.lv2/"
export INSTALLED="libjack.0.dylib"
export ARCH

follow_dependencies () {
  libname=$1
  dependencies=`otool -arch all -L "$libname"  | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'`
  for l in $dependencies; do
    depname=`basename $l`
    deppath=`dirname $l`
    if [ ! -f "$depname" ]; then
      deploy_lib $depname "$deppath"
    fi
  done
}

update_links () {
  libname=$1
  libpath=$2
  for n in `ls $TARGET_DEPLOY_DIR/*.dylib`; do
    install_name_tool -change "$libpath/$libname" @loader_path/$libname "$n"
  done
}

deploy_lib () {
  libname=$1
  libpath=$2
  check=`echo $INSTALLED | grep $libname`
  if [ "X$check" = "X" ]; then
    if [ ! -f "$TARGET_DEPLOY_DIR/$libname" ]; then
      cp -f "$libpath/$libname" "$TARGET_DEPLOY_DIR/$libname.ALL"
      lipo "$TARGET_DEPLOY_DIR/$libname.ALL" -thin $ARCH -output "$TARGET_DEPLOY_DIR/$libname" || \
        cp "$TARGET_DEPLOY_DIR/$libname.ALL" "$TARGET_DEPLOY_DIR/$libname"
      rm "$TARGET_DEPLOY_DIR/$libname.ALL"
      install_name_tool -id @loader_path/$libname "$TARGET_DEPLOY_DIR/$libname"
      follow_dependencies $libname
    fi
    export INSTALLED="$INSTALLED $libname"
  fi
  update_links $libname $libpath
}

rm -rf "$TARGET_DEPLOY_DIR"
mkdir -p "$TARGET_DEPLOY_DIR"
cp -v "$TARGET_BUILD_DIR/"*.dylib "$TARGET_DEPLOY_DIR"

cd $TARGET_DEPLOY_DIR 
for libname in *.dylib; do
  install_name_tool -id "$libname" "$libname"
done

MORELIBS=`otool -arch all -L *.dylib | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'`
while [ "X$MORELIBS" != "X" ]; do
  for l in $MORELIBS; do
    libname=`basename $l`
    libpath=`dirname $l`
    deploy_lib "$libname" "$libpath"
  done
  MORELIBS=`otool -arch all -L *.dylib | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'`
done

cp "$TARGET_BUILD_DIR/"*.ttl "$TARGET_DEPLOY_DIR/"

cd "$TLD"

rm -f meters.lv2-osx-$ARCH-`git describe --tags`.zip
zip -r meters.lv2-osx-$ARCH-`git describe --tags`.zip meters.lv2/
ls -l meters.lv2-osx-$ARCH-`git describe --tags`.zip
