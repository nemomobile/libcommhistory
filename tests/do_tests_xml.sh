#!/bin/bash
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

TARGET_FILE=${1/%\//}/tests.xml
TEST_PKG_NAME=${2}
TEST_FOLDERS=${3}

# we need exactly 3 arguments
if [ ! "$#" = "3" ]
then
    echo "usage: do_tests_xml.sh <tests.xml target path> <test package name> <list of test folders>"
    exit 1
fi

cat > ${TARGET_FILE} << EOF
<?xml version="1.0" encoding="ISO-8859-1"?>
    <testdefinition version="0.1">
    <suite name="${TEST_PKG_NAME}">
EOF

echo "Copying test_set.xml files from the following test folders $TEST_FOLDERS for target $TARGET_FILE"
echo $TEST_FOLDERS

for test_folder in ${TEST_FOLDERS}
do
    	TEST_SET_FILE=${test_folder}/test_set.xml
    if [ -f "${TEST_SET_FILE}" ]
    then
        echo " Adding test set descriptions for ${test_folder}..."
        cat ${TEST_SET_FILE} >> ${TARGET_FILE}
    fi
done

cat >> ${TARGET_FILE} << EOF
    </suite>
</testdefinition>


<!-- End of File -->

EOF

# End of File
