HEADERS += trackerio.h \
           commonutils.h \
           eventmodel.h \
           queryrunner.h \
           eventmodel_p.h \
           event.h \
           messagepart.h \
           callevent.h \
           eventtreeitem.h \
           conversationmodel.h \
           callmodel.h \
           callmodel_p.h \
           draftmodel.h \
           smsinboxmodel.h \
           syncsmsmodel.h \
           syncsmsmodel_p.h \
           outboxmodel.h \
           groupmodel.h \
           groupmodel_p.h \
           group.h \
           adaptor.h \
           unreadeventsmodel.h \
           conversationmodel_p.h \
           classzerosmsmodel.h \
           mmscontentdeleter.h \
           updatequery.h \
           contactlistener.h \
           libcommhistoryexport.h \
           idsource.h \
           trackerio_p.h \
           queryresult.h \
           singleeventmodel.h \
           committingtransaction.h \
           committingtransaction_p.h \
           eventsquery.h \
           preparedqueries.h \
           updatesemitter.h \
           constants.h \
           groupobject.h \
           groupmanager.h \
           groupmanager_p.h \
           contactgroupmodel.h \
           contactgroupmodel_p.h \
           contactgroup.h

contains(DEFINES, COMMHISTORY_USE_QTCONTACTS_API) {
    HEADERS += qcontacttpmetadata_p.h
}

SOURCES += trackerio.cpp \
           commonutils.cpp \
           eventmodel.cpp \
           queryrunner.cpp \
           eventmodel_p.cpp \
           eventtreeitem.cpp \
           conversationmodel.cpp \
           callmodel.cpp \
           draftmodel.cpp \
           smsinboxmodel.cpp \
           syncsmsmodel.cpp \
           outboxmodel.cpp \
           groupmodel.cpp \
           group.cpp \
           adaptor.cpp \
           event.cpp \
           messagepart.cpp \
           unreadeventsmodel.cpp \
           classzerosmsmodel.cpp \
           mmscontentdeleter.cpp \
           contactlistener.cpp \
           idsource.cpp \
           queryresult.cpp \
           singleeventmodel.cpp \
           committingtransaction.cpp \
           eventsquery.cpp \
           updatequery.cpp \
           updatesemitter.cpp \
           groupmanager.cpp \
           groupobject.cpp \
           contactgroupmodel.cpp \
           contactgroup.cpp
