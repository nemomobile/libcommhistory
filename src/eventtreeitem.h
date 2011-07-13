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

#ifndef COMMHISTORY_EVENTTREEITEM_H
#define COMMHISTORY_EVENTTREEITEM_H

#include <QList>

namespace CommHistory {

class Event;

/*!
 * \class EventTreeItem
 *
 * Event container for CommHistoryModels.
 */
class EventTreeItem
{
public:
    EventTreeItem(const Event &event, EventTreeItem *parent = 0);
    ~EventTreeItem();

    void appendChild(EventTreeItem *child);
    void prependChild(EventTreeItem *child);
    void insertChildAt(int row, EventTreeItem *child);
    void moveChild(int fromRow, int toRow);
    void removeAt(int row);
    EventTreeItem *child(int row);
    Event &eventAt(int row);
    int childCount() const;
    Event &event();
    void setEvent(const Event &event);
    EventTreeItem *parent();
    int row() const;

private:
    QList<EventTreeItem *> children;
    Event *eventData;
    EventTreeItem *parentItem;
};

}

#endif
