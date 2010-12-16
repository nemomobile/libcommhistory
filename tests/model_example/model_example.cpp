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

#include <QApplication>
#include <QTableView>
#include "groupmodel.h"
#include "conversationmodel.h"

using namespace CommHistory;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    ConversationModel model;

    GroupModel groupModel;
    groupModel.setQueryMode(EventModel::SyncQuery);
    groupModel.getGroups("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");

    model.getEvents(groupModel.group(groupModel.index(0, 0)).id());

    QTableView view;
    view.setModel(&model);
    view.show();

#if 0
    // Examples for accessing group data. You'll have to either use
    // SyncQuery mode or wait for rowsInserted() or modelReady() before
    // you can iterate over the model data.

    for (int i = 0; i < model.rowCount(); i++) {
        // Model style:
        qDebug() << model.index(i, EventModel::EventId).data().toString() <<
            model.index(i, EventModel::StartTime).data().toDateTime() <<
            model.index(i, EventModel::LocalUid).data().toString() <<
            model.index(i, EventModel::RemoteUid).data().toString() <<
            model.index(i, EventModel::FreeText).data().toString();

        // Event style:
        Event e = model.event(model.index(i, 0));
        // or
        // Event e = model.index(i, 0).data(Qt::UserRole).value<Event>();

        qDebug() << e.id() << e.startTime() << e.localUid() <<
            e.remoteUid() << e.freeText();

    }
#endif

    int ret = app.exec();

    return ret;
}
