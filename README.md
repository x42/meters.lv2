meters.lv2 - Audio Level Meters
===============================

meters.lv2 is a collection of audio-level meters with GUI in LV2 plugin format.

It includes needle style meters (mono and stereo variants)

*   IEC 60268-10 Type I / DIN
*   IEC 60268-10 Type I / Nordic
*   IEC 60268-10 Type IIa / BBC
*   IEC 60268-10 Type IIb / EBU
*   IEC 60268-17 / VU

and the following stereo plugins

*   Stereo Phase Correlation Meter (Needle Display)
*   EBU R128 Meter with Histogram and History
*   True-Peak (4x Oversampling)
*   Goniometer
*   31 Band Spectrum-Analyzer


Install
-------

Compiling these plugin requires the LV2 SDK, gnu-make, a c-compiler,
gtk+2.0, libpango and libcairo.

```bash
  git clone git://github.com/x42/meters.lv2.git
  cd meters.lv2
  make
  sudo make install PREFIX=/usr
  
  # test run
  jalv.gtk 'http://gareus.org/oss/lv2/meters#DINmono'
```

Note to packagers: The Makefile honors `PREFIX` and `DESTDIR` variables as well
as `CFLAGS`, `LDFLAGS` and `OPTIMIZATIONS` (additions to `CFLAGS`).

Screenshots
-----------

![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/LV2ebur128.png "EBU R128 Meter GUI")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/LV2meters.png "Various Needle Meters in Ardour")

