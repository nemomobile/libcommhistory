/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#ifndef COMMHISTORY_CONTACTRESOLVER_H
#define COMMHISTORY_CONTACTRESOLVER_H

#include <QObject>
#include "event.h"
#include "contactlistener.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class ContactResolverPrivate;

class LIBCOMMHISTORY_EXPORT ContactResolver : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ContactResolver)

public:
    explicit ContactResolver(QObject *parent);
    
    void appendEvents(const QList<Event> &events);
    void prependEvents(const QList<Event> &events);

    QList<Event> events() const;
    bool isResolving() const;

signals:
    void eventsResolved(const QList<Event> &events);
    void finished();

private:
    ContactResolverPrivate *d_ptr;
};

}

#endif
