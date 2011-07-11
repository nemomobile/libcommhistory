/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#ifndef COMMHISTORY_IDSOURCE_H
#define COMMHISTORY_IDSOURCE_H

#include <QObject>
#include <QSharedMemory>
#include <QFile>

namespace CommHistory {

struct IdSourceData;

class IdSource : public QObject
{
    Q_OBJECT

public:
    explicit IdSource(QObject *parent = 0);
    ~IdSource();

    int nextEventId();
    int nextGroupId();

    void setNextEventId(int eventId);
    void setNextGroupId(int groupId);

private:
    bool openSharedMemory();
    void save(IdSourceData *data);
    void load(IdSourceData *data);
    IdSourceData* lockSharedData();
    int skipToNextBasket(int currentId);
    bool newBasket(int currentId) const;

private:
    QSharedMemory m_IdSource;
    QFile m_File;
};

} // namespace
#endif // COMMHISTORY_IDSOURCE_H
