#!/bin/bash
#
# Set the MAC address of any/all ethernet cards
# for DECnet
#
# Parameters:
#
# $1 - DECnet address in the usual area.node format
# $2 - List of ethernet card names to set or "all"
#

# Use the "ip" command if available
if [ -x /sbin/ip ]; then
  IPCMD="/sbin/ip"
fi

calc_ether()
{
  MACADDR=""

  ADDR=`echo $1 | sed -n 's/\([0-9]*\.[0-9]*\)/\1/p'`
  AREA=`echo $ADDR | sed -n 's/\([0-9]*\)\.\([0-9]*\)/\1/p'`
  NODE=`echo $ADDR | sed -n 's/\([0-9]*\)\.\([0-9]*\)/\2/p'`

  [ -z "$AREA" ] && AREA=0
  [ -z "$NODE" ] && NODE=0

  if [ "$NODE" -le 1023 -a  "$NODE" -ge 1 -a "$AREA" -le 63 -a "$AREA" -ge 1 ]
  then
    NUM=$(($AREA*1024 + $NODE))
    MACADDR="`printf \"AA:00:04:00:%02X:%02X\" $((NUM%256)) $((NUM/256))`"
  else
    exit 1
  fi
  return 0
}

if [ -z "$2" ]
then
  echo ""
  echo "usage: $0 <decnet-addr> <network interfaces>|all"
  echo ""
  echo " eg.   $0 1.9 eth0 "
  echo " eg.   $0 1.9 all"
  echo ""
  exit 1
fi

calc_ether $1
if [ $? != 0 ]
then
  exit 1
fi

CARDS=$2
if [ "$CARDS" = "all" -o "$CARDS" = "ALL" ]
then
  CARDS=`cat /proc/net/dev|grep eth|cut -f1 -d':'`
fi

set_default=""
for i in $CARDS
do
  if [ -n "$IPCMD" ]
  then
    $IPCMD link set $i address $MACADDR
    $IPCMD link set $i up
  else
    ifconfig $i hw ether $MACADDR allmulti up
  fi

  if [ -z "$set_default" -a -f /proc/sys/net/decnet/default_device ]
  then
    echo $i >/proc/sys/net/decnet/default_device
    set_default="DONE"
  fi
done

