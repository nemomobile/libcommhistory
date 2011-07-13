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

#include <QDebug>

#include "eventmodel_p.h"
#include "group.h"
#include "eventsquery.h"

#include "singleeventmodel.h"

namespace CommHistory {

using namespace CommHistory;

class SingleEventModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(SingleEventModel);

    SingleEventModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
        queryLimit = 1;
        clearUrl();
        clearTokens();
    }

    bool acceptsEvent(const Event &event) const {

        // If the urls match, we'll accept
        if (!m_url.isEmpty() && m_url == event.url()) {
            return true;
        }

        // If the m_groupId is valid and the m_groupId doesn't
        // match events group id, don't accept the event
        if (m_groupId != -1 && m_groupId != event.groupId()) {
            return false;
        }

        // Accept the event if message tokens or mmsIds match
        return (!m_token.isEmpty() && m_token == event.messageToken()) ||
               (!m_mmsId.isEmpty() && m_mmsId == event.mmsId());
    }

    void clearUrl() {
        m_url.clear();
    }

    void clearTokens() {
        m_token.clear();
        m_mmsId.clear();
        m_groupId = -1;
    }

    QUrl m_url;
    QString m_token;
    QString m_mmsId;
    int m_groupId;
};

SingleEventModel::SingleEventModel(QObject *parent)
    : EventModel(*new SingleEventModelPrivate(this), parent)
{
}

SingleEventModel::~SingleEventModel()
{
}

bool SingleEventModel::getEventByUri(const QUrl &uri)
{
    Q_D(SingleEventModel);

    reset();
    d->clearEvents();
    d->clearTokens();

    d->m_url = uri;

    EventsQuery query(d->propertyMask);

    query.addPattern(QString(QLatin1String("FILTER(%2 = <%1>) ")).arg(uri.toString()))
            .variable(Event::Id);

    return d->executeQuery(query);
}


bool SingleEventModel::getEventByTokens(const QString &token,
                                        const QString &mmsId,
                                        int groupId)
{
    Q_D(SingleEventModel);

    reset();
    d->clearEvents();
    d->clearUrl();

    d->m_token = token;
    d->m_mmsId = mmsId;
    d->m_groupId = groupId;

    EventsQuery query(d->propertyMask);

    QStringList pattern;
    if (!token.isEmpty()) {
        pattern << QString(QLatin1String("{ %2 nmo:messageId \"%1\" }")).arg(token);
    }
    if (!mmsId.isEmpty()) {
        pattern << QString(QLatin1String("{ %2 nmo:mmsId \"%1\"; nmo:isSent true }")).arg(mmsId);
    }

    query.addPattern(pattern.join(QLatin1String("UNION"))).variable(Event::Id);
    if (groupId > -1)
        query.addPattern(QString(QLatin1String("%2 nmo:communicationChannel <%1> ."))
                         .arg(Group::idToUrl(groupId).toString()))
        .variable(Event::Id);

    query.setDistinct(true);

    return d->executeQuery(query);
}

} // namespace CommHistory
