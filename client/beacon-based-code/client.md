# client_new.c

Program ini dibuat berdasarkan program daemon beacon.c yang tersedia dari library ax25-tools:

```
# apt-get install ax25-tools
```

Alur program ini adalah sebagai berikut:

1. Identifikasi port AX.25 yang tersedia, ambil data callsign dari port AX.25 yang terdapat pada Interface Linux

2. Konversi callsign berupa string sesuai dengan standar addressing pada AX.25

3. Menjalankan fungsi daemon

4. Looping program socket, pengiriman data, dan close socket setiap interval tertentu.

## penjelasan program

Program ini memerlukan library:
```
#include <sys/types.h>
#include <sys/socket.h>
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

#include <config.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
```

Fungsi-fungsi dasar yang menjadi dasar pada program ini adalah:
1.
