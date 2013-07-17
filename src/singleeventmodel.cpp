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
#include <QSqlQuery>
#include <QSqlError>

#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "eventmodel_p.h"
#include "group.h"

#include "singleeventmodel.h"

namespace CommHistory {

using namespace CommHistory;

class SingleEventModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(SingleEventModel);

    SingleEventModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
        queryLimit = 1;
        m_eventId = -1;
        clearTokens();
    }

    bool acceptsEvent(const Event &event) const {

        // If the urls match, we'll accept
        if (m_eventId >= 0 && event.id() == m_eventId)
            return true;

        // If the m_groupId is valid and the m_groupId doesn't
        // match events group id, don't accept the event
        if (m_groupId != -1 && m_groupId != event.groupId()) {
            return false;
        }

        // Accept the event if message tokens or mmsIds match
        return (!m_token.isEmpty() && m_token == event.messageToken()) ||
               (!m_mmsId.isEmpty() && m_mmsId == event.mmsId());
    }

    void clearTokens() {
        m_token.clear();
        m_mmsId.clear();
        m_groupId = -1;
    }

    int m_eventId;
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

bool SingleEventModel::getEventById(int eventId)
{
    Q_D(SingleEventModel);

    beginResetModel();
    d->clearEvents();
    d->clearTokens();
    endResetModel();

    d->m_eventId = eventId;

    QSqlQuery query = DatabaseIOPrivate::instance()->createQuery();
    QString q = DatabaseIOPrivate::eventQueryBase() + QString::fromLatin1(" WHERE id = %1").arg(eventId);

    if (!query.prepare(q)) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return d->executeQuery(query);
}

bool SingleEventModel::getEventByTokens(const QString &token,
                                        const QString &mmsId,
                                        int groupId)
{
    Q_D(SingleEventModel);

    beginResetModel();
    d->clearEvents();
    d->m_eventId = -1;
    endResetModel();

    d->m_token = token;
    d->m_mmsId = mmsId;
    d->m_groupId = groupId;

    QSqlQuery query = DatabaseIOPrivate::instance()->createQuery();

    QString q = DatabaseIOPrivate::eventQueryBase();
    q += "WHERE ";

    if (groupId > -1) { 
        q += QString::fromLatin1("groupId = %1 ").arg(groupId);

        if (!token.isEmpty() || !mmsId.isEmpty())
            q += "AND ";
    }

    if (!token.isEmpty()) {
        q += "( messageToken = :messageToken ";
        if (!mmsId.isEmpty())
            q += " OR ";
    }

    if (!mmsId.isEmpty())
        q += QString::fromLatin1("( mmsId = :mmsId AND direction = %1 ) ").arg(Event::Outbound);

    if (!token.isEmpty())
        q += " ) ";

    if (!query.prepare(q)) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    if (!token.isEmpty())
        query.bindValue(":messageToken", token);
    if (!mmsId.isEmpty())
        query.bindValue(":mmsId", mmsId);

    return d->executeQuery(query);
}

} // namespace CommHistory
