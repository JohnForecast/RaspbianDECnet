#!/bin/sh
#
# Configure /etc/decnet.conf
#

# Use a temp file
if [ -x /bin/tempfile ]
then
  CONFFILE="`tempfile`"
else
  CONFFILE="/tmp/decnet.conf$$"
fi
REALFILE=/etc/decnet.conf

if [ -f "$REALFILE" ]
then
  echo "The file $REALFILE already exists. Not overwriting it"
  exit 0
fi

control_c()
{
  rm -f $CONFFILE
  echo
  echo "DECnet configuration abandoned"
  exit 1
}

trap "control_c" SIGINT

# Validate the DECnet node address
validate_addr()
{
  if [ -z "$1" ]
  then
    return 0
  fi

  if [ `expr $1 : '[0-9]*\.[0-9]*'` -ne "`expr length $1`" ]
  then
    echo "The node address must be in the format area.node"
    return 0
  fi

  area=`echo $1|cut -d. -f1`
  node=`echo $1|cut -d. -f2`
  if [ "$area" -le 0 -o "$area" -ge 64 ]
  then
    echo 'Area must be between 1 and 63 inclusive'
  fi

  if [ "$node" -le 0 -o "$node" -ge 1024 ]
  then
    echo 'Node must be between 1 and 1024 inclusive'
  fi
  
  if [ "$node" -gt 0 -a "$node" -lt 1024 -a "$area" -gt 0 -a "$area" -lt 64 ]
  then
    return 1
  fi
  return 0
}

# Just check the name is 6 characters or less.
validate_name()
{
   if [ "`expr length $1`" -gt 6 ]
   then
    echo "DECnet node names must be less than six characaters long"
    return 0
   else
    return 1
   fi
}

# Check the interface name is in /proc/net/dev
validate_iface()
{
  grep -q "$1:" /proc/net/dev
  if [ $? = 0 ]
  then
    return 1
  else
    echo "Can't find device $1 on your system. Choose one of the following:"
    awk '/.*:/ { print substr($1,0,index($1,":")-1) }' < /proc/net/dev
    return 0
  fi
}

echo
echo "DECnet configuration."
echo "Press control/C to cancel."
echo

#
# Get the current host (executor) information
#
if [ -f $REALFILE ]
then
  def_addr="`awk '/^executor/ { print $2 }' <$REALFILE`"
  def_name="`awk '/^executor/ { print $4 }' <$REALFILE`"
  def_iface="`awk '/^executor/ { print $6 }' <$REALFILE`"
else
  def_name=`hostname -s|cut -b1-6`
  def_addr="1.1"
  def_iface="eth0"
fi

addr_valid=0
name_valid=0
iface_valid=0
while [ $addr_valid = "0" ]
do
  echo -n "Enter your DECnet node address [$def_addr] : "
  read addr
  if [ -z "$addr" ]
  then
    addr=$def_addr
  fi
  validate_addr $addr
  addr_valid=$?
done

while [ $name_valid = "0" ]
do
  echo -n "Enter your DECnet node name [$def_name] : "
  read name
  if [ -z "$name" ]
  then
    name=$def_name
  fi
  validate_name $name
  name_valid=$?
done

while [ $iface_valid = "0" ]
do
  echo -n "Enter your Ethernet interface name [$def_iface] : "
  read interface
  if [ -z "$interface" ]
  then
    interface=$def_iface
  fi
  validate_iface $interface
  iface_valid=$?
done

#
# Start generating the file with our local node information in it.
#
cat >$CONFFILE <<EOF
#
#               DECnet hosts file
#
#Node           Node            Name            Node    Line    Line
#Type           Address         Tag             Name    Tag     Device
#-----          -------         -----           -----   -----   ------
EOF
printf >>$CONFFILE "executor         %-7s        name            %-6s  line    %s\n" $addr $name $interface

#-----------------------------------------------------------------------------

#
# Now add some nodes
#
echo
echo 'You may now add some other DECnet nodes on your network. When you have'
echo 'finished, press [ENTER] when prompted for the node address'
echo 

while [ 1 ]
do
  addr_valid=0
  name_valid=0
  while [ $addr_valid = "0" ]
  do
    echo -n "Enter the node's address: area.node (eg 1.1) : "
    read addr
    if [ -z "$addr" ]
    then
      break 2
    fi
    validate_addr $addr
    addr_valid=$?
  done

  while [ $name_valid = "0" ]
  do
    echo -n "Enter it's node name                         : "
    read name
    validate_name $name
    name_valid=$?
  done
  printf >>$CONFFILE "node             %-7s        name            %-6s\n" $addr $name
  echo

done

#
# Confirm copying the file to /etc
#
echo
cat $CONFFILE
echo 
echo "This is your DECnet configuration file. Press [ENTER] to make it"
echo "live or Control/C to exit"
read dummy
cp $CONFFILE $REALFILE
if [ $? -ne 0 ]
then
  echo
  echo "Could not copy file to $REALFILE, you must be root to do this."
  echo "This script will exit now, leaving your new file saved as"
  echo "$CONFFILE"
  echo
  echo "su to root and copy this file to $REALFILE to complete installation"
  echo
  exit
fi
rm -f $CONFFILE

exit
