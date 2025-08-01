#! /bin/bash

###############################################################################
#
# Copyright (C) 2025 by Jonathan Naylor G4KLX
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
# Full path to the M17Hosts file
#
M17HOSTS=/path/to/M17Hosts.txt

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

# Download the M17Hosts.txt file
curl --fail --silent -S -L -o  ${M17HOSTS} -A "M17Gateway - G4KLX" https://hostfiles.refcheck.radio/M17Hosts.txt

exit 0
