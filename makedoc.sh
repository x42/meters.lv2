#!/bin/sh

export OUTDIR=/tmp/meterdoc/
export KEEPOUTDIR=1

lv2ls \
	| grep gareus \
	| grep meter \
	| grep -v _gtk \
	| ~/data/coding/lv2toweb/tools/create_doc.sh

CUTLN=$(cat -n $OUTDIR/index.html | grep '<!-- start of page !-->' | cut -f 1 | tr -d ' ')
# TODO test if $CUTLN is an integer

head -n $CUTLN $OUTDIR/index.html > $OUTDIR/index.html_1

cat >> $OUTDIR/index.html_1 << EOF
<h1>Meters.lv2</h1>
<img src="vu.png" alt="" style="float:left; width: 140px;"/>
<div style="margin-left:150px;"><p>
<a href="https://github.com/x42/meters.lv2">meters.lv2</a> - a collection of audio level meters wit GUI in LV2 plugin format.
The bundle includes needle style meters (mono and stereo variants) of various IEC 60268 standards as well as related meters.
Usage should be pretty much self-explanatory.
</p></div>
<div style="clear:both;"></div>
<p>A few notes:</p>
<ul>
<li>click + drag on the calibration-screw allows to modify the reference level of the needle meters</li>
<li>shift + click on the calibration-screw resets to default</li>
<li>clicking anywhere on the bar-graph meters resets the peak-hold</li>
<li>To facilitate chaining, all plugins pass the audio though unmodified (they mereley tap-off the signal, host providing this is zero-copy)</li>
</ul>
EOF

tail -n +$CUTLN $OUTDIR/index.html >> $OUTDIR/index.html_1
mv $OUTDIR/index.html_1 $OUTDIR/index.html

rm doc/html/http__*
cp -a $OUTDIR/*.html doc/html/
cp -a $OUTDIR/*.png doc/html/

echo -n "git add+push doc? [y/N] "
read -n1 a
echo
if test "$a" != "y" -a "$a" != "Y"; then
	exit
fi

cd doc/html && git add *.html *.png && git commit -m "update documentation" && git push
