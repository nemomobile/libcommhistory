include( ../common-project-config.pri )
include( ../common-vars.pri )

TARGET = commhistory-declarative
PLUGIN_IMPORT_PATH = org/nemomobile/commhistory
VERSION = $$PROJECT_VERSION

INCLUDEPATH += ../src 

SOURCES += src/plugin.cpp \
    src/callproxymodel.cpp \
    src/conversationproxymodel.cpp \
    src/declarativegroupmanager.cpp \
    src/sharedbackgroundthread.cpp \
    src/contactaddresslookup.cpp

HEADERS += src/constants.h \
    src/callproxymodel.h \
    src/conversationproxymodel.h \
    src/declarativegroupmanager.h \
    src/sharedbackgroundthread.h \
    src/contactaddresslookup.h \
    src/debug.h

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

equals(QT_MAJOR_VERSION, 4) {
    LIBS += -L../src ../src/libcommhistory.so
    QT += declarative
    CONFIG += mobility
    MOBILITY += contacts
    PKGCONFIG += qtcontacts-sqlite-extensions contactcache
    target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
}

equals(QT_MAJOR_VERSION, 5) {
    LIBS += -L../src ../src/libcommhistory-qt5.so
    QT += qml contacts
    PKGCONFIG += qtcontacts-sqlite-qt5-extensions contactcache-qt5
    DEFINES += USING_QTPIM
    target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
}

INSTALLS += target

qmldir.files += $$PWD/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir
