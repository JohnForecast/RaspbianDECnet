#!/bin/sh
#
# Check the kernel headers available to us. 
#
#

# But not for production builds. we assume a 2.4 kernel
if [ -n "$RELEASE" ]
then
  return 0
fi

rm -f include/netdnet/dn.h

if [ -f /usr/src/linux/include/netdnet/dn.h ]
then
  #
  # Eduardo's kernel - only use dn.h if it doesn't define nodeent
  # (which belongs in dnetdb.h)
  #
  grep -q nodeent /usr/src/linux/include/netdnet/dn.h 
  if [ $? = 1 ]
  then
    echo Using dn.h from Eduardo\'s kernel
    cp  /usr/src/linux/include/netdnet/dn.h include/netdnet
  else
    echo Using dn.h from our distribution
    cp include/kernel/netdnet/dn.h include/netdnet
  fi
fi

if [ -f /usr/src/linux/include/linux/dn.h ]
then
  #
  # Steve's kernel
  #
  echo Using dn.h from Steve\'s kernel
  cp  /usr/src/linux/include/linux/dn.h include/netdnet
fi

if [ ! -f include/netdnet/dn.h ]
then
  #
  # Use our fallback include file
  #
  echo Assuming 2.4+ kernel
  cp include/kernel/netdnet/dn.h include/netdnet
fi
return 0
