#!/bin/sh
# start ax25 on /dev/ttyACM0 using lora port in /etc/ax25/axports
/usr/sbin/kissattach -m 224 /dev/ttyACM0 lora
sleep 1
# Set KISS TNC to No CRC
/usr/sbin/kissparms -p lora -c 1
sleep 1
# Start AX.25 Daemon
/usr/sbin/ax25d
sleep 1
# listen for stations heard
/usr/sbin/mheardd
# end of script
