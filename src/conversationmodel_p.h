/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef COMMHISTORY_CONVERSATIONMODEL_P_H
#define COMMHISTORY_CONVERSATIONMODEL_P_H

#include "eventmodel_p.h"
#include "conversationmodel.h"
#include "group.h"

namespace CommHistory
{

class ConversationModelPrivate : public EventModelPrivate {
public:
    Q_OBJECT
    Q_DECLARE_PUBLIC(ConversationModel);

    ConversationModelPrivate(EventModel *model);

    void updateEvents(const QList<Event::Contact> &contacts,
                      const QString &remoteUid);
    bool acceptsEvent(const Event &event) const;
    bool fillModel(int start, int end, QList<CommHistory::Event> events);
    EventsQuery buildQuery() const;
    bool isModelReady() const;

public Q_SLOTS:
    void groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups);
    virtual void modelUpdatedSlot(bool successful);
    void extraReceivedSlot(QList<CommHistory::Event> events, QVariantList extra);
    void groupsDeletedSlot(const QList<int> &groupIds);
    void contactSettingsChangedSlot(const QHash<QString, QVariant> &changedSettings);

public:
    int filterGroupId;
    Event::EventType filterType;
    QString filterAccount;
    Event::EventDirection filterDirection;
    bool firstFetch;
    uint eventsFilled;
    uint lastEventTrackerId;

    int activeQueries;
};

}


#endif
