#!/bin/sh

#******************************************************************************
#   (C) John Forecast                           john@forecast.name
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#******************************************************************************

# Shell script to download and build DECnet on Raspbian and piOS.
#
# This file should be placed in, an initially empty, directory which will
# be used to hold the source files for the Linux kernel and DECnet. If it
# finds the Linux or the DECnet source tree present, it will ask if it
# should down load a new version.
#
# When invoking this script it is possible to override certain features
# by setting environment variables:
#
#       branch=xxx ./BuildAndInstall.sh
#
# The following overrides are available:
#
#  branch=name          Use the specified branch when downloading the
#                       Linux kernel source tree
#
#  forcekernel=name     Force the build to generate the specified kernel
#                       This is only valid when running on a 32-bit kernel
#                       and "name" can be one of the following:
#
#                               kernel   - Original Pi, Zero and Compute Module
#                               kernel7  - Pi 2, Pi 3 and Compute Module 3
#                               kernel7l - Pi 4 and Compute Module 4

APTGET=/usr/bin/apt-get
CAT=/bin/cat
CHMOD=/bin/chmod
CP=/bin/cp
CUT=/usr/bin/cut
DATE=/bin/date
DPKG=/usr/bin/dpkg
EXPR=/usr/bin/expr
GIT=/usr/bin/git
GREP=/bin/grep
MAKE=/usr/bin/make
MV=/bin/mv
PRINTF=/usr/bin/printf
PWD=/bin/pwd
RM=/bin/rm
SED=/bin/sed
SUDO=/usr/bin/sudo
TRUE=/bin/true

Here=`$PWD`
Log=$Here/Log

NoRPI=1
Model=
Revision=
RealModel=
Arch=`uname -m`
ArchDir=arm
ArchSw=
CPUSw=-j1
Kernel=
BootKernel=
Config=

LinuxDownload=1
DECnetDownload=1
DECnetConfig=1
Pause=1

Name=
Addr=
Area=0
Node=0
Interface=

BranchSw=

#
# Useful functions
#
check_addr() {
    if [ ! -z "$1" ]; then
        if [ `$EXPR $1 : '[0-9]*\.[0-9]*'` -ne "`$EXPR length $1`" ]; then
            echo "Node address must be in the format area.node"
            return 0
        fi

        AreaNo=`echo $1 | $CUT -d. -f1`
        NodeNo=`echo $1 | $CUT -d. -f2`

        if [ "$AreaNo" -le 0 -o "$AreaNo" -ge 64 ]; then
            echo "Area must be between 1 and 63 inclusive"
            return 0
        fi

        if [ "$NodeNo" -le 0 -o "$NodeNo" -ge 1024 ]; then
            echo "Node must be between 1 and 1023 inclusive"
            return 0
        fi
        return 1
    fi
    return 0
}

check_name() {
    if [ "`$EXPR length $1`" -le 6 ]; then
        if [ `$EXPR $1 : '[0-9a-zA-Z]*'` -ne "`$EXPR length $1`" ]; then
            echo "DECnet node names may be up to 6 alphanumeric characters"
            return 0
        fi
        return 1
    fi
    return 0
}

check_interface() {
    $GREP -q "$1:" /proc/net/dev
    if [ $? -ne 0 ]; then
        echo "Can't find device $1 on your system. Choose one of the following:"
        awk '/.*:/ { print substr($1,0,index($1, ":")) }' < /proc/net/dev
        return 0
    fi
    return 1
}

DOCMD() {
    echo "$1" >> $Log
$1 >> $Log 2>&1
return $?
}

if [ -e /sys/firmware/devicetree/base/model ]; then
    $GREP -q "^Raspberry Pi" /sys/firmware/devicetree/base/model
    NoRPI=$?
fi

if [ $NoRPI -ne 0 ]; then
    echo "This script is not running on a Raspberry Pi"
    exit 1
fi

#
# Handle some command-line overrides
#
if [ ! -z "$branch" ]; then
   BranchSw="--branch=$branch"
fi

#
# Make sure all the packages we will need are installed
#
echo "Checking required packages are installed"
for pkg in git bc bison flex libssl-dev make libncurses-dev
do
    $SUDO $DPKG -s $pkg >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo -n "Installing $pkg .... "
        $SUDO $APTGET install -y $pkg >/dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo "Failed to install package '" $pkg "'"
            exit 1
        fi
        echo
    fi
done

#
# Determine which model/revision we're running on
#
temp=`$CUT -d " " -f 3- /sys/firmware/devicetree/base/model`
echo $temp | $GREP -q "Rev"
if [ $? -eq 0 ]; then
    Model=`echo $temp | awk '{ sub(" Rev .*$", "", $0); print $0 }'`
    Revision=`echo $temp | awk '{ sub("^.* Rev ", "", $0); print $0 }'`
else
    Model=$temp
fi
RealModel=$Model

if [ ! -z "$forcekernel" ]; then
    Model=$forcekernel
fi

#
# Determine which kernel we need to build
#
if [ "$Arch" != "aarch64" ]; then
    Kernel=zImage
    case $Model in
        "Model A"*|"Model B"*|"Zero"*|"Compute Module"|kernel)
            case $RealModel in
                "Model A"*|"Model B"*|"Zero"*|"Compute Module")
                    ;;

                *)
                    CPUSw=-j4
                    ;;
            esac
            BootKernel=kernel
            Config=bcmrpi_defconfig
            ;;

        "2 Model"*|"3 Model"*|"Compute Module 3"*|kernel7)
            CPUSw=-j4
            BootKernel=kernel7
            Config=bcm2709_defconfig
            ;;

        "4 Model"*|"Compute Module 4"*|kernel7l)
            CPUSw=-j4
            BootKernel=kernel7l
            Config=bcm2711_defconfig
            ;;

        *)
            echo "Unknown Raspberry Pi model - '" $temp "'"
            echo "This script may need updating"
            exit 1
    esac
else
    Kernel=Image
    BootKernel=kernel8
    Config=bcm2711_defconfig
    ArchDir=arm64
    ArchSw="ARCH=arm64"
    CPUSw=-j4
fi

## Temp for debug
#echo "Model:          " $RealModel
#echo "Revision:       " $Revision
#if [ ! -z "$forcekernel" ]; then
#    echo "Forcekernel:    " $forcekernel
#fi
#echo "Kernel:         " $Kernel
#echo "BootKernel:     " $BootKernel
#echo "Config:         " $Config
#echo "Build switches: " $ArchSw " " $CPUSw
##

if [ -d ./linux ]; then
    while $TRUE ; do
        echo "There appears to be an existing linux source tree present"
        echo "Do you want to:"
        echo "  1 - Delete existing tree, download a new one and build"
        echo "  2 - Clean and rebuild using the existing tree"
        echo "  3 - Install the kernel/modules/dtbs already built in the existing tree"
        echo "  4 - Don't bother to build or install the kernel/modules/dtbs"
        echo
        read -p "Enter code (1 - 4): " LinuxDownload Junk

        if [ "$Junk" = "" ]; then
            case $LinuxDownload in
                1|2|3|4)
                    break
                    ;;
            esac
        fi
        echo "Invalid Response"
        echo
    done
fi

if [ -d ./RaspbianDECnet ]; then
    while $TRUE ; do
        echo
        echo "There appears to be an existing DECnet source tree present"
        echo "Do you want to:"
        echo "  1 - Delete existing tree, download a new one and build"
        echo "  2 - Clean and rebuild using the existing tree"
        echo "  3 - Install the DECnet utilties already built in the existing tree"
        echo "  4 - Don't bother to build or install the DECnet utilities"
        echo
        read -p "Enter code (1 - 4): " DECnetDownload Junk

        if [ "$Junk" = "" ]; then
            case $DECnetDownload in
                1|2|3|4)
                    break
                    ;;
            esac
        fi
        echo "Invalid Response"
        echo
    done
fi

if [ -z "$forcekernel" ]; then
    if [ -e /etc/decnet.conf ]; then
        while $TRUE ; do
            echo
            echo "There appears to to an existing DECnet configuration present"
            echo "Do you want to:"
            echo "  1 - Delete the existing configuration files and create new ones"
            echo "  2 - Use the existing configuration files"
            echo
            read -p "Enter code (1 - 2): " DECnetConfig Junk
            
            if [ "$Junk" = "" ]; then
                case $DECnetConfig in
                    1|2)
                        break
                        ;;
                esac
            fi
            echo "Invalid Response"
            echo
        done
    fi
else
    DECnetConfig=3
fi

if [ -z "$forcekernel" ]; then
    echo
    while $TRUE; do
        echo "When the build completes, do you want to:"
        echo "  1 - Install the new kernel and DECnet on this system"
        echo "  2 - Pause before installing the new kernel and DECnet on this system"
        echo "  3 - Terminate this script"
        read -p "Enter code (1 - 3): " PostBuild Junk

        if [ "$Junk" = "" ]; then
            case $PostBuild in
                1|2|3)
                    break
                    ;;
            esac
        fi
        echo "Invalid Response"
        echo
    done
else
    PostBuild=3
fi

DefaultName=`hostname -s | cut -b1-6`
DefaultAddr="1.1"
DefaultInterface="eth0"

if [ $PostBuild -ne 3 -a $DECnetConfig -eq 1 ]; then
    echo
    while $TRUE; do
        read -p "Enter your DECnet node address [$DefaultAddr] : " Addr
        if [ -z "$Addr" ]; then
            Addr=$DefaultAddr
        fi
        check_addr $Addr
        if [ $? -eq 1 ]; then break; fi
    done
    Area=$AreaNo
    Node=$NodeNo

    while $TRUE; do
        read -p "Enter your DECnet node name [$DefaultName] : " Name
        if [ -z "$Name" ]; then
            Name=$DefaultName
        fi
        check_name $Name
        if [ $? -eq 1 ]; then break; fi
    done

    while $TRUE; do
        read -p "Enter your Ethernet/Wireless interface name [$DefaultInterface] : " Interface
        if [ -z "$Interface" ]; then
            Interface=$DefaultInterface
        fi
        check_interface $Interface
        if [ $? -eq 1 ]; then break; fi
    done

    $CP /dev/null /tmp/nodes$$

    echo
    echo "You may now set up some other DECnet nodes on your network. When you"
    echo "have finished, press [ENTER] when prompted for the node address."
    echo

    while $TRUE; do
        echo
        while $TRUE; do
            read -p "Enter the node's address: area.node (e.g. 1.1) : " remaddr
            if [ -z "$remaddr" ]; then
                break 2
            fi
            check_addr $remaddr
            if [ $? -eq 1 ]; then break; fi
        done

        while $TRUE; do
            read -p "Enter its node name                            : " remname
            check_name $remname
            if [ $? -eq 1 ]; then break; fi
        done
        printf >>/tmp/nodes$$ "node             %-7s        name            %-6s\n" $remaddr $remname
    done
fi

echo
echo "All questions have been answered"
if [ $LinuxDownload -ne 4 -a $DECnetDownload -ne 4 ]; then
    echo "The download/build log will be in $Log"
fi

start=`$DATE`

echo "Kernel/DECnet build started at $start\n" > $Log

if [ $LinuxDownload -eq 1 ]; then
    DOCMD "$RM -rf linux"
    DOCMD "$GIT clone --depth=1 $BranchSw https://github.com/raspberrypi/linux"
    if [ $? -ne 0 ]; then
        echo "git failed to clone linux repository"
        exit 1
    fi
    
    DOCMD "cd $Here/linux"
    DOCMD "$MAKE $ArchSw $Config"
    if [ $? -ne 0 ]; then
        echo "Failed to make default configuration"
        exit 1
    fi

    $SED '
/CONFIG_DECNET is not set/ c\
CONFIG_DECNET=m\
# CONFIG_DECNET_ROUTER is not set\
# CONFIG_DECNET_NF_GRABULATOR is not set
' <.config >.config.new
    
    $MV .config .config.old
    $MV .config.new .config
fi

if [ $DECnetDownload -eq 1 ]; then
    DOCMD "cd $Here"
    DOCMD "$RM -rf RaspbianDECnet"
    DOCMD "$GIT clone https://github.com/JohnForecast/RaspbianDECnet"
    if [ $? -ne 0 ]; then
        echo "git failed to clone RaspbianDECnet repository"
        exit 1
    fi
fi

if [ $LinuxDownload -le 2 ]; then
    DOCMD "cd $Here"

    DOCMD "$CP ./RaspbianDECnet/kernel/include/net/*.h ./linux/include/net"
    DOCMD "$RM -rf ./linux/net/decnet"
    DOCMD "$CP -r ./RaspbianDECnet/kernel/net/decnet ./linux/net"

    DOCMD "cd $Here/linux"
    DOCMD "$MAKE clean"
    DOCMD "$MAKE $CPUSw $ArchSw $Kernel modules dtbs"
    if [ $? -ne 0 ]; then
        echo "Kernel make failed"
        exit 1
    fi
fi

if [ $DECnetDownload -le 2 ]; then
    DOCMD "cd $Here/RaspbianDECnet/dnprogs"
    DOCMD "$MAKE clean"
    DOCMD "$MAKE $CPUSw all"
    if [ $? -ne 0 ]; then
        echo "DECnet Utilities make failed"
        exit 1
    fi
    
    if [ "$Arch" != "aarch64" ]; then
        for a in armv6l armv7 armv7l; do
            if [ "$Arch" != "$a" ]; then
                ln -sf $Arch libvaxdata/linux/$a
            fi
        done
    fi
    ln -sf $Arch libvaxdatalin
fi

echo "Kernel and/or DECnet Utilities build complete"
case $PostBuild in
     1)
     ;;

     2)
     read -p "Press [ENTER] when ready" Junk
     ;;

     3)
     exit 0
     ;;
esac

if [ $LinuxDownload -le 3 ]; then
    DOCMD "cd $Here/linux"
    DOCMD "$SUDO $MAKE modules_install"
    if [ $? -ne 0 ]; then
        echo "Kernel modules install failed"
        exit 1
    fi

    if [ "$Arch" = "aarch64" ]; then
        DOCMD "$SUDO $CP arch/$ArchDir/boot/dts/broadcom/*.dtb /boot"
    else
        DOCMD "$SUDO $CP arch/$ArchDir/boot/dts/*.dtb /boot"
    fi
    
    DOCMD "$SUDO $CP arch/$ArchDir/boot/dts/overlays/*.dtb* /boot/overlays"
    DOCMD "$SUDO $CP arch/$ArchDir/boot/dts/overlays/README /boot/overlays"
    DOCMD "$SUDO $CP arch/$ArchDir/boot/$Kernel /boot/$BootKernel.img"
fi

if [ $DECnetDownload -le 3 ]; then
    DOCMD "cd $Here/RaspbianDECnet/dnprogs"
    DOCMD "$SUDO $MAKE install"
    if [ $? -ne 0 ]; then
        echo "DECnet Utilities install failed"
        exit 1
    fi
fi

if [ $DECnetConfig -eq 1 ]; then
    NodeAddr=`expr \( $Area \* 1024 \) + $Node`
    byte4=`$EXPR $NodeAddr % 256`
    byte5=`$EXPR $NodeAddr / 256`
    
    $PRINTF >/tmp/$$.link "[Match]\n"
    $PRINTF >>/tmp/$$.link "OriginalName=%s\n\n" $Interface
    $PRINTF >>/tmp/$$.link "[Link]\n"
    $PRINTF >>/tmp/$$.link "MACAddress=aa:00:04:00:%02x:%02x\n" $byte4 $byte5
    $PRINTF >>/tmp/$$.link "NamePolicy=kernel database onboard slot path\n"

    $SUDO $MV /tmp/$$.link /etc/systemd/network/00-mac.link

    $CAT >/tmp/$$.conf <<EOF
#V001.0
#               DECnet hosts file
#
#Node           Node            Name            Node    Line    Line
#Type           Address         Tag             Name    Tag     Device
#-----          -------         -----           -----   -----   ------
EOF
printf >>/tmp/$$.conf "executor         %-7s        name            %-6s  line   %s\n" $Addr $Name $Interface

    $CAT /tmp/nodes$$ >>/tmp/$$.conf
    $RM /tmp/nodes$$
    $SUDO $MV /tmp/$$.conf /etc/decnet.conf
    $SUDO $CHMOD 644 /etc/decnet.conf
fi

echo
echo "Kernel and/or DECnet Utilities successfully installed"
echo
echo "You may still need to decide how to start up DECnet - see section 6 of"
echo "README.Raspbian in $Here/RaspbianDECnet"
echo "A reboot is required to change the MAC address of $Interface and"
echo "clear out any currently loaded DECnet module"
echo
