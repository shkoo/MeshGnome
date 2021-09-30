#!/bin/sh

set -e

BOARD=esp8266:esp8266:d1_mini_pro

arduino-cli compile -v -e -b ${BOARD} BlinkCount

for USB in /dev/ttyUSB*
do
arduino-cli upload -b ${BOARD} -p ${USB}  &
done

wait

