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

#include <QtGui>
#include "groupmodel.h"
#include "group.h"
#include "event.h"

using namespace CommHistory;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    GroupModel model;

    model.getGroups();
    // If you want to filter:
    // getGroups("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0",
    //           "user@gmail.com")

    QTableView view;
    view.setModel(&model);
    view.show();

#if 0
    // Examples for accessing group data. You'll have to either use
    // SyncQuery mode or wait for rowsInserted() or modelReady() before
    // you can iterate over the model.

    for (int i = 0; i < model.rowCount(); i++) {
        // Model style:
        qDebug() << model.index(i, GroupModel::GroupId).data().toInt() <<
            model.index(i, GroupModel::LocalUid).data().toString() <<
            model.index(i, GroupModel::LastEventId).data().toInt();

        // Group style:
        Group g = model.group(model.index(i, 0));
        // or
        g = model.index(i, 0).data(GroupModel::GroupRole).value<Group>();
        qDebug() << g.lastMessageText();
    }
#endif

    int ret = app.exec();

    return ret;
}
