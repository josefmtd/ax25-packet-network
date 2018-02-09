# ax25digipeater

axdigi adalah program yang dibuat untuk melaksanakan fungsi digipeater sederhana, yakni single port digipeater, di mana program ini akan mendengarkan paket ax25 yang diterima oleh sistem linux, dan menguji masing-masing paket jika sesuai dengan kriteria digipeater. Jika ya, maka program akan mengirimkan kembali paket yang diterima, sesuai dengan aturan digipeater dari protokol AX.25 2.2.

axdigi is a program designed to function as a simple digipeater, or to be exact, a single port digipeater, where this program will open a socket that listens for ax.25 packet received by a Linux system, and inspected each packet to see if it fits the criterion for digipeating. If criterion is met, the program will re-send the received packet with regards to digipeater algorithm definition by AX.25 2.2 protocol.

## To Do List

- [x] Created a stable version that does ax.25 digipeating
- [ ] Daemonized the program
- [ ] Added error handling

## How To Compile This Program

To compile this program, you would need to get the latest ax.25 kernel by using these commands:
```
# apt-get install libax25-dev libax25 ax25-tools ax25-apps
```

You can add this program to your Linux computer by using these commands:
```
$ git clone https://github.com/josefmtd/ax25digipeater
$ cd ax25digipeater
$ gcc axdigi.c /usr/lib/libax25.a -o axdigi
```

## Using the program

To use the program make sure you have done the following:

```
# nano /etc/ax25/axports
```
and add your radio settings to the ax25 ports, for example I'm using an OpenTracker 3m with USB KISS, this is how my axports looks like:

>1       YD0SHY-3        19200   255     7       OpenTracker USB KISS (1200 bps)

The format is: name, callsign, speed(serial baudrate), paclen(maximum packet allowed), window(maximum outstanding packet), and description

Connect your KISS TNC to your computer and you can attach the kiss modem to your ax0 interface in Linux by using this command:
```
# kissattach <device tty> <port name>
```
e.g.
```
# kissattach /dev/ttyACM0 1
```

To run my digipeater program, go to your cloned git-repository folder and run this command:
```
# ./axdigi
```
