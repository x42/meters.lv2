#!/bin/bash

if test -n "$1" ; then
DESTDIR="$1"
else
DESTDIR=/tmp/meters
fi

pushd "`/usr/bin/dirname \"$0\"`" > /dev/null; this_script_dir="`pwd`"; popd > /dev/null
MTRROOT="${this_script_dir}/.."

set -e

rm -rf ${DESTDIR}/x42Meters.app
mkdir -p ${DESTDIR}/x42Meters.app/Contents/MacOS
mkdir -p ${DESTDIR}/x42Meters.app/Contents/Resources

cat > ${DESTDIR}/x42Meters.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>x42Meters</string>
	<key>CFBundleName</key>
	<string>x42Meters</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleSignature</key>
	<string>~~~~</string>
	<key>CFBundleVersion</key>
	<string>1.0</string>
	<key>CFBundleIconFile</key>
	<string>x42-meters</string>
	<key>CSResourcesFileMapped</key>
	<true/>
</dict>
</plist>
EOF

cp -vi ${MTRROOT}/tools/osxwrapper.sh ${DESTDIR}/x42Meters.app/Contents/MacOS/x42Meters
cp -vi ${MTRROOT}/tools/x42-meters.icns ${DESTDIR}/x42Meters.app/Contents/Resources/
cp -vi ${MTRROOT}/x42/x42-meter-collection ${DESTDIR}/x42Meters.app/Contents/MacOS/
