###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
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

#-----------------------------------------------------------------------------
# doc.pri
#-----------------------------------------------------------------------------

# list of documentation folders to install
DOC_FOLDERS = doc/html \
              doc/qch

# files and folders listed in the installation target's .files section
# must exist _before_ qmake generates the Makefile...so, make sure our
# documentation target folders exist in the current build folder
for( folder, DOC_FOLDERS ) {
    system( mkdir -p $$(PWD)/$${folder} )
}


#-----------------------------------------------------------------------------
# extra build targets for generating and cleaning documentation
#-----------------------------------------------------------------------------
for( subdir, SUBDIRS) {
    DOC_INPUT += $${_PRO_FILE_PWD_}/$${subdir}
}

# target for generating documentation
doctarget.target     = docs
doctarget.commands   = OUTPUT_DIRECTORY=doc \
                       PROJECT_NAME=\"$${PROJECT_NAME}\" \
                       PROJECT_NUMBER=\"$${PROJECT_VERSION}\" \
                       STRIP_FROM_PATH=\"$${_PRO_FILE_PWD_}\" \
                       INPUT=\"$${DOC_INPUT}\" \
                       QHP_NAMESPACE=\"com.nokia.example.$${TARGET}\" \
                       QHP_VIRTUAL_FOLDER=\"$${TARGET}\" \
                       TAGFILES=\"$$system(pwd)/qt.tags\" \
                       TAGFILE=\"doc/$${TARGET}.tags\" \
                       doxygen $$system(pwd)/doxy.conf
doctarget.depends    = FORCE
QMAKE_EXTRA_TARGETS += doctarget


# target for cleaning generated documentation
doccleantarget.target = cleandocs
for( folder, DOC_FOLDERS ) {
    doccleantarget.commands += rm -r -f $${folder};
}
doccleantarget.commands += rm -r -f doc/libeventlogger.tags;
doccleantarget.depends   = FORCE
QMAKE_EXTRA_TARGETS     += doccleantarget


#-----------------------------------------------------------------------------
# installation setup
# NOTE: remember to set headers.files before this include to have the headers
# properly setup.
#-----------------------------------------------------------------------------
include( ../common-installs-config.pri )


#-----------------------------------------------------------------------------
# Installation target setup for documentation
#-----------------------------------------------------------------------------
documentation.path = $${INSTALL_PREFIX}/share/doc/$${PROJECT_NAME}
for( folder, DOC_FOLDERS ) {
    documentation.files += $${OUT_PWD}/$${folder}
}

INSTALLS              += documentation
message("====")
message("==== INSTALLS += documentation")


# End of File

