#!/bin/sh

export OUTDIR=/tmp/meterdoc/
export KEEPOUTDIR=1
export KEEPIMAGES=1

lv2ls \
	| grep gareus \
	| grep meter \
	| grep -v _gtk \
	| ~/data/coding/lv2toweb/tools/create_doc.sh

CUTLN=$(cat -n $OUTDIR/index.html | grep '<!-- start of page !-->' | cut -f 1 | tr -d ' ')
# TODO test if $CUTLN is an integer

head -n $CUTLN $OUTDIR/index.html > $OUTDIR/index.html_1

cat >> $OUTDIR/index.html_1 << EOF
<div style="margin:1em auto; width:66em; background-color:#fff; padding:1em;">
<h1>Meters.lv2</h1>
<img src="vu.png" alt="" style="float:left; width: 140px;"/>
<div style="margin-left:150px;">
<p>
<a href="https://github.com/x42/meters.lv2">meters.lv2</a> - a collection of audio level meters with GUI in <a href="lv2plug.in">LV2 plugin</a> format.
</p><p>
The bundle includes needle style meters (mono and stereo variants) as well as related audio measurement tools.
The needle meters comply with
IEC 60268-<a href="http://webstore.iec.ch/webstore/webstore.nsf/ArtNum_PK/18760">10</a>/<a href="http://webstore.iec.ch/webstore/webstore.nsf/artnum/001383">17</a> specifications, the K-meters are RMS type as conform with <a href="http://www.aes.org/technical/documentDownloads.cfm?docID=65">Bob Katz</a>' practices. IEC <a href="http://webstore.iec.ch/webstore/webstore.nsf/artnum/019426">61260</a> standard filters are used for the 1/3 octave spectrum display and the EBU Recommendation 128 applied to the ebur128 meter.
</p></div>
<div style="clear:both;"></div>
<p>A few notes on the user interface:</p>
<ul>
<li>Click + drag on the calibration-screw allows to modify the reference level of the needle meters.</li>
<li>Shift + click on the calibration-screw resets to default.</li>
<li>Ctrl + click anywhere on the canvas of a needle meter resets its window to 100% size.</li>
<li>Clicking anywhere on the bar-graph meters resets the peak-hold.</li>
<li>To facilitate chaining, all plugins pass the audio though unmodified (they merely tap-off the signal; host providing this is zero-copy). The plugins can be dropped into the effect-processor box of any DAW supporting LV2.</li>
</ul>
<p>Further reading:</p>
<ul>
<li>IEC standards linked from text above (non free)</li>
<li>Wikipedia: <a href="http://en.wikipedia.org/wiki/VU_meter">VU_meter</a>, <a href="http://en.wikipedia.org/wiki/Peak_programme_meter">Peak_programme_meter</a>.</li>
<li><a href="http://www.digido.com/how-to-make-better-recordings-part-2.html">How To Make Better Recordings in the 21st Century - An Integrated Approach to Metering, Monitoring, and Leveling Practices</a> by Bob Katz.</li>
<li>"Audio Metering: Measurements, Standards and Practice: Measurements, Standards and Practics", by Eddy Brixen. ISBN: 0240814673</li>
<li>"Art of Digital Audio", by John Watkinson. ISBN: 0240515870</li>
</ul>

EOF

tail -n +$CUTLN $OUTDIR/index.html \
	| sed 's/<!-- end of page !-->/<\/div>/' \
	>> $OUTDIR/index.html_1
mv $OUTDIR/index.html_1 $OUTDIR/index.html

rm doc/html/http__*.html
cp -a $OUTDIR/*.html doc/html/
if test -n "$KEEPIMAGES"; then
	cp -an $OUTDIR/*.png doc/html
else
	cp -a $OUTDIR/*.png doc/html/
fi

echo -n "git add+push doc? [y/N] "
read -n1 a
echo
if test "$a" != "y" -a "$a" != "Y"; then
	exit
fi

cd doc/html && git add *.html *.png && git commit -m "update documentation" && git push
