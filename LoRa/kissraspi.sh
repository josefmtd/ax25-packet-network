# !/bin/sh
# start ax25 on USB0 using port defined in /etc/ax25/axports
/usr/sbin/kissattach -m 232 /dev/ttyUSB0 lora
sleep 1
# Set KISS Mode to No CRC
/usr/sbin/kissparms -p lora -c 1
sleep 1
# Echo max window size and maximum packet length
echo 1 > /proc/sys/net/ax25/ax0/standard_window_size
echo 232 > /proc/sys/net/ax25/ax0/maximum_packet_length
sleep 1
# Begin AX25.d
/usr/sbin/ax25d
sleep 1
# Listen for stations heard
/usr/sbin/mheardd
#end of script
