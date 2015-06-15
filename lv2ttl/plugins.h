#define MULTIPLUGIN 1
#define X42_MULTIPLUGIN_NAME "Meter Collection"
#define X42_MULTIPLUGIN_URI "http://gareus.org/oss/lv2/meters"

#include "lv2ttl/cor.h"
#include "lv2ttl/dr14stereo.h"
#include "lv2ttl/ebur128.h"
#include "lv2ttl/goniometer.h"
#include "lv2ttl/k20stereo.h"
#include "lv2ttl/phasewheel.h"
#include "lv2ttl/sigdisthist.h"
#include "lv2ttl/spectr30.h"
#include "lv2ttl/stereoscope.h"
#include "lv2ttl/tp_rms_stereo.h"
#include "lv2ttl/bitmeter.h"

static const RtkLv2Description _plugins[] = {
	_plugin_cor,
	_plugin_dr14,
	_plugin_ebur,
	_plugin_goniometer,
	_plugin_k20stereo,
	_plugin_phasewheel,
	_plugin_sigdisthist,
	_plugin_spectr30,
	_plugin_stereoscope,
	_plugin_tprms2,
	_plugin_bitmeter,
};
