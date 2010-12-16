#!/bin/sh
###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

# addcalls.sh count
#
# adds <count> calls with random phone numbers and types
# (dialed/received/missed). Requires perl.

TYPE[0]=dialed
TYPE[1]=received
TYPE[2]=missed

for i in $(seq 1 $1); do
    NUMBER=$(perl -le "print int(rand(100000000))+1;")
    commhistory-tool addcall ring/tel/ring $NUMBER ${TYPE[$((RANDOM % 3))]}
done
