include( ../common-project-config.pri )
include( ../common-vars.pri )

TARGET = commhistory-declarative
PLUGIN_IMPORT_PATH = org/nemomobile/commhistory
VERSION = $$PROJECT_VERSION

LIBS += -L../src \
    ../src/libcommhistory.so
INCLUDEPATH += ../src 

SOURCES += src/plugin.cpp \
    src/callproxymodel.cpp \
    src/groupobject.cpp \
    src/groupproxymodel.cpp \
    src/conversationproxymodel.cpp

HEADERS += src/constants.h \
    src/callproxymodel.h \
    src/groupobject.h \
    src/groupproxymodel.h \
    src/conversationproxymodel.h

# do not edit below here, move this to a shared .pri?
TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT += declarative

target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$PWD/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir
