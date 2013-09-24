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

#include <QtDBus/QtDBus>
#include "classzerosmsmodel.h"
#include "eventmodel_p.h"
#include "event.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

using namespace CommHistory;

class LIBCOMMHISTORY_EXPORT ClassZeroSMSModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(ClassZeroSMSModel);

    ClassZeroSMSModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
    }

    bool acceptsEvent(const Event &event) const {
        if( event.type() == Event::ClassZeroSMSEvent) {
            return true;
        }
        return false;
    }

    virtual void addToModel(Event &event)
    {
        Q_Q(ClassZeroSMSModel);

        EventModelPrivate::addToModel(event);
        if (acceptsEvent(event)) {
            emit q->newMessage(event.messageToken(), event.freeText());
        }
    }
};

LIBCOMMHISTORY_EXPORT ClassZeroSMSModel::ClassZeroSMSModel(QObject *parent)
    : EventModel(*new ClassZeroSMSModelPrivate(this), parent)
{
}

void ClassZeroSMSModel::clear()
{
    Q_D(ClassZeroSMSModel);
    d->clearEvents();
}

} // namespace CommHistory
