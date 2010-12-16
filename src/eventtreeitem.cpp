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

#include <QDebug>
#include <QList>
#include "event.h"
#include "eventtreeitem.h"

using namespace CommHistory;

EventTreeItem::EventTreeItem(const Event &event, EventTreeItem *parent)
{
    parentItem = parent;
    eventData = new Event( event );
}

EventTreeItem::~EventTreeItem()
{
    delete eventData;
    qDeleteAll(children);
}

void EventTreeItem::appendChild(EventTreeItem *child)
{
    children.append(child);
}

void EventTreeItem::prependChild(EventTreeItem *child)
{
    children.prepend(child);
}

void EventTreeItem::moveChild( int fromRow, int toRow )
{
    // if the given index is out of range or the two index are the same
    if ( fromRow < 0 || childCount() - 1 < fromRow ||
         toRow < 0 || childCount() -1 < toRow ||
         fromRow == toRow )
    {
        return;
    }

    children.insert( toRow, children.takeAt( fromRow ) );
}

void EventTreeItem::insertChildAt(int row, EventTreeItem *child)
{
    children.insert(row, child);
}

void EventTreeItem::removeAt(int row)
{
    delete children.takeAt(row);
}

EventTreeItem *EventTreeItem::child(int row)
{
    return children.value(row);
}

Event &EventTreeItem::eventAt(int row)
{
    return children.value(row)->event();
}

int EventTreeItem::childCount() const
{
    return children.count();
}

Event &EventTreeItem::event()
{
    return *eventData;
}

void EventTreeItem::setEvent(const Event &event)
{
    if ( eventData )
    {
        delete eventData;
    }
    eventData = new Event( event );
}

EventTreeItem *EventTreeItem::parent()
{
    return parentItem;
}

int EventTreeItem::row() const
{
    if (parentItem) {
        return parentItem->children.indexOf(const_cast<EventTreeItem *>(this));
    }

    return 0;
}
