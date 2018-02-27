#!/bin/sh
# start ax25 on /dev/ttyACM0 using lora port in /etc/ax25/axports
/usr/sbin/kissattach -m 232 /dev/ttyACM0 lora
sleep 1
# Set KISS TNC to No CRC
/usr/sbin/kissparms -p lora -c 1
sleep 1
# Set max packet and window size
echo 1 > /proc/sys/net/ax25/ax0/standard_window_size
echo 232 >  /proc/sys/net/ax25/ax0/maximum_packet_length
# Start AX.25 Daemon
/usr/sbin/ax25d
sleep 1
# listen for stations heard
/usr/sbin/mheardd
# end of script
