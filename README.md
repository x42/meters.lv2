meters.lv2 - Audio Level Meters
===============================

meters.lv2 is a collection of audio-level meters with GUI in LV2 plugin format.

It includes needle style meters (mono and stereo variants)

*   IEC 60268-10 Type I / DIN
*   IEC 60268-10 Type I / Nordic
*   IEC 60268-10 Type IIa / BBC
*   IEC 60268-10 Type IIb / EBU
*   IEC 60268-17 / VU

Stereo & Mono variants of bar-graph meters:

*   30 Band 1/3 octave spectrum analyzer IEC 61260
*   Digital True-Peak Meter (4x Oversampling), Type II rise-time, 13.3dB/s falloff.
*   True-Peak (4x Oversampling) + RMS (600ms integration time) combined with numeric readout
*   K-12, K-14, K-20 / RMS type K-Meters according to the K-system introduced by Bob Katz
*   DR-14 (crest factor / loudness range measurement method)

and the following stereo plugins:

*   EBU R128 Meter with Histogram and History
*   Stereo Phase Correlation Meter (Needle Display)
*   BBC Mid/Side M-6 (Needle Display)
*   Goniometer (Stereo Phase Scope)
*   Phase/Frequency Wheel
*   Stereo/Frequency Monitor

as well as a mono:

*   Signal Distribution Histogram
*   Bitmeter

Usage
-----

Should be pretty much self-explanatory. A few notes:

* click + drag on the calibration-screw allows to modify the reference
	level of the needle meters
* shift + click on the calibration-screw resets to default
* ctrl + click on meters with scalable GUI resets the window-size to 100%
* clicking anywhere on the bar-graph meters resets the peak-hold

Install
-------

### Requirements

Building the complete plugin collection and the standalone JACK applications
requires:

| Dependency | `pkg-config` name | Used for |
| --- | --- | --- |
| LV2 SDK 1.6.0 or newer | `lv2` | Plugin API and idle interface |
| GLib | `glib-2.0` | Utility functions |
| Cairo and Pango | `cairo`, `pangocairo`, `pango` | Meter rendering and text |
| FFTW single-precision library | `fftw3f` | Spectrum analysis |
| JACK development library | `jack` | Standalone meter applications |
| OpenGL and GLU (Linux) | `gl`, `glu` | Plugin user interfaces |

A C compiler, a C++ compiler, GNU Make, `pkg-config`, and the Git client are
also needed. Distribution package names vary; development packages commonly
end in `-dev` or `-devel`. In particular, FFTW must provide the
single-precision `fftw3f` library, not only `fftw3`.

After installing the dependencies, this command checks the libraries that the
Linux build expects:

```console
pkg-config --exists lv2 glib-2.0 cairo pangocairo pango fftw3f jack gl glu
```

A successful check prints nothing and exits with status zero.

### Compile and install

```console
git clone --recursive https://github.com/x42/meters.lv2.git
cd meters.lv2
make
sudo make install PREFIX=/usr
```

For an existing clone made without `--recursive`, run `make submodules` before
`make` to fetch the required `robtk` source.

Note to packagers: The Makefile honors `PREFIX` and `DESTDIR` variables as well
as `CFLAGS`, `LDFLAGS` and `OPTIMIZATIONS` (additions to `CFLAGS`), also
see the first 10 lines of the Makefile.
You really want to package the superset of [x42-plugins](https://github.com/x42/x42-plugins).


Note on build-dependencies
--------------------------

These plugins count on rather recent (Jan 2013) fixes^Wfeatures of
some libraries (but may work with older versions too):

In particular multi-threading in cairo, pixman and pango.
Versions of those libraries earlier than libcairo < 1.12.10,
libpixman < 0.30.2 and libpango < 1.32.6 are not thread-safe.
As with all concurrency issues, things may or may not work and
if you only open one meter GUI at a time it's usually fine.

Also note that the plugins use the LV2 idle-interface (lv2 >= 1.4.2)
The plugin-host (eg. ardour or qtractor) needs to be compiled with
this or a later version of the LV2 SDK to support the features.

The plugin-host must also support http://lv2plug.in/ns/ext/resize-port/


Screenshots
-----------

![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/LV2ebur128.png "EBU R128 Meter GUI")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/LV2meters.png "Various Needle Meters in Ardour")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/spectr_and_goni.png "Spectrum Analyzer and Stereo Phase Scope")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/phasewheel.png "Phase/Frequency Wheel")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/dr14meter.png "DR-14 Crest Factor Meter")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/stereoscope.png "Stereo/Frequency Scope")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/sigdisthist.png "Signal Distribution Histogram")
![screenshot](https://raw.github.com/x42/meters.lv2/master/doc/bitmeter.png "Bit Meter")
