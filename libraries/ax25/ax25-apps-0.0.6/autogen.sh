#!/bin/sh
#
# autogen.sh glue for ax25-tools
# $Id: autogen.sh,v 1.1 2002/03/04 01:26:44 csmall Exp $
#
# Requires: automake, autoconf, dpkg-dev
set -e

# Refresh GNU autotools toolchain.
aclocal 
autoheader
automake --verbose --foreign --add-missing

# The automake package already links config.sub/guess to /usr/share/misc/
for i in config.guess config.sub missing install-sh mkinstalldirs ; do
	test -r /usr/share/automake/${i} && cp -f /usr/share/automake/${i} .
	chmod 755 ${i}
done

autoconf

exit 0
