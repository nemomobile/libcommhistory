/* Copyright (C) 2012 John Brooks <john.brooks@dereferenced.net>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "groupobject.h"
#include "groupmodel.h"

using namespace CommHistory;

GroupObject::GroupObject(QObject *parent)
    : QObject(parent), model(0)
{
}

GroupObject::GroupObject(const Group &group, QAbstractItemModel *parent)
    : QObject(parent), Group(group), model(parent)
{
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            SLOT(modelDataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            SLOT(modelRowsRemoved(QModelIndex,int,int)));
}

void GroupObject::modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft.parent() == bottomRight.parent());

    for (int r = topLeft.row(); r <= bottomRight.row(); r++) {
        const Group &g = model->index(r, 0, topLeft.parent()).data(GroupModel::GroupRole).value<Group>();

        if (g.id() != id())
            continue;

        Group old(static_cast<Group&>(*this));
        Group::operator=(g);

        if (old.localUid() != localUid())
            emit localUidChanged();
        if (old.remoteUids() != remoteUids())
            emit remoteUidsChanged();
        if (old.chatType() != chatType())
            emit chatTypeChanged();
        if (old.chatName() != chatName())
            emit chatNameChanged();
        if (old.startTime() != startTime())
            emit startTimeChanged();
        if (old.endTime() != endTime())
            emit endTimeChanged();
        if (old.totalMessages() != totalMessages())
            emit totalMessagesChanged();
        if (old.unreadMessages() != unreadMessages())
            emit unreadMessagesChanged();
        if (old.sentMessages() != sentMessages())
            emit sentMessagesChanged();
        if (old.lastEventId() != lastEventId())
            emit lastEventIdChanged();
        if (old.contacts() != contacts())
            emit contactsChanged();
        if (old.lastMessageText() != lastMessageText())
            emit lastMessageTextChanged();
        if (old.lastVCardFileName() != lastVCardFileName())
            emit lastVCardFileNameChanged();
        if (old.lastVCardLabel() != lastVCardLabel())
            emit lastVCardLabelChanged();
        if (old.lastEventStatus() != lastEventStatus())
            emit lastEventStatusChanged();
        if (old.lastModified() != lastModified())
            emit lastModifiedChanged();

        break;
    }
}

void GroupObject::modelRowsRemoved(const QModelIndex &parent, int start, int end)
{
    for (int r = start; r <= end; r++) {
        QModelIndex index = model->index(r, GroupModel::GroupId, parent);
        if (index.data().toInt() != id())
            continue;

        qDebug() << Q_FUNC_INFO << "group removed";
        emit groupRemoved();
        break;
    }
}

