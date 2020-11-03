#! /bin/bash

###############################################################################
#
# M17Hostspdate.sh
#
# Copyright (C) 2020 by Jonathan Naylor G4KLX
# Copyright (C) 2016 by Tony Corbett G0WFV
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
###############################################################################
#
# On a Linux based system, such as a Raspberry Pi, this script will perform all 
# the steps required to maintain the M17Hosts.txt (or similar) file for you.
#
# It is designed to run from crontab and will download the latest hosts file.
#
# The gateway will refresh its hosts files at regular intervals as specified in the
# gateway ini file.
#
# To install in root's crontab use the command ...
#
#     sudo crontab -e
#
# ... and add the following line to the bottom of the file ...
#
#     0  0  *  *  *  /path/to/script/M17HostsUpdate.sh 1>/dev/null 2>&1
#
# ... where /path/to/script/ should be replaced by the path to this script.
#
###############################################################################
#
#                              CONFIGURATION
#
# Full path to the M17 hosts file, without final slash
M17HOSTSPATH=/path/to/M17/hosts/file
M17HOSTSFILE=${M17HOSTSPATH}/M17Hosts.txt

DATABASEURL='http://www.dudetronics.com/ar-dns/M17Hosts.txt'

###############################################################################
#
# Do not edit below here
#
###############################################################################

# Check we are root
if [ "$(id -u)" != "0" ] 
then
	echo "This script must be run as root" 1>&2
	exit 1
fi

# Get new file
curl ${DATABASEURL} 2>/dev/null > ${M17HOSTSFILE}

