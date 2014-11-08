#!/bin/sh

if test ! -x /usr/local/bin/jackd -a ! -x /usr/bin/jackd ; then
  /usr/bin/osascript -e '
    tell application "Finder"
    display dialog "You do not have JACK installed. This application will not run without it. See http://jackaudio.org/ for further info." buttons["OK"]
    end tell'
  exit 1
fi

meter=$(/usr/bin/osascript << EOT
  tell application "Finder"
  activate
  set x42meters to { "EBU R128", "K20/RMS", "True-Peak and RMS Meter", "DR14 - Dynamic Range", "Goniometer", "Phase-Correlation", "Phase/Frequency Wheel", "Stereo/Frequency Scope", "Signal Distribution Histogram", "Spectrum Analyzer" }
  return (choose from list x42meters with title "x42-Meters" with prompt "Select Meter Type:")
  end
EOT)

if [ $? != 0 -o $meter = "false" ]
then
  #echo "canceled"
  exit 0
fi

curdir=`dirname "$0"`
cd "${curdir}"
exec "./x42-meter-collection" "$meter"
