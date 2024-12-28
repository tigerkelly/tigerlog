#!/bin/bash

. /home/pi/bin/common.sh

CompareBinFile tigerlog

echo Starting tigerlog.
exec /home/pi/bin/tigerlog

exit 0
