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

#include "groupproxymodel.h"
#include "groupobject.h"

#include "groupmodel.h"

using namespace CommHistory;

GroupProxyModel::GroupProxyModel(QObject *parent)
    : QIdentityProxyModel(parent)
{
}

void GroupProxyModel::setSourceModel(QAbstractItemModel *m)
{
    if (m == sourceModel())
        return;

    GroupModel *g = qobject_cast<GroupModel*>(m);
    if (model && !g) {
        qWarning() << Q_FUNC_INFO << "Model must be a CommHistory::GroupModel";
        m = 0;
    }

    if (model)
        disconnect(model, 0, this, 0);

    model = g;
    QIdentityProxyModel::setSourceModel(model);

    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            SLOT(sourceDataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
            SLOT(sourceRowsRemoved(QModelIndex,int,int)));

    emit sourceModelChanged();
}

GroupObject *GroupProxyModel::group(int row)
{
    if (!model) {
        qWarning() << Q_FUNC_INFO << "No group model instance";
        return 0;
    }

    Group g = model->group(mapToSource(index(row, 0)));
    if (!g.isValid())
        return 0;

    GroupObject *re = groupObjects.value(g.id());
    if (!re) {
        re = new GroupObject(g, this);
        groupObjects.insert(g.id(), re);
    }

    return re;
}

GroupObject *GroupProxyModel::groupById(int id)
{
    if (!model) {
        qWarning() << Q_FUNC_INFO << "No group model instance";
        return 0;
    }

    GroupObject *re = groupObjects.value(id);
    if (re)
        return re;

    for (int r = 0; r < model->rowCount(); r++) {
        Group g = model->group(model->index(r, 0));
        if (!g.isValid() || g.id() != id)
            continue;

        re = new GroupObject(g, this);
        groupObjects.insert(g.id(), re);
        break;
    }

    return re;
}

void GroupProxyModel::sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft.parent() == bottomRight.parent());

    for (int r = topLeft.row(); r <= bottomRight.row(); r++) {
        const Group &g = model->group(model->index(r, 0));

        GroupObject *obj = groupObjects.value(g.id());
        if (!obj)
            continue;

        obj->updateGroup(g);
    }
}

void GroupProxyModel::sourceRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);

    for (int r = start; r <= end; r++) {
        const Group &g = model->group(model->index(r, 0));
        GroupObject *obj = groupObjects.value(g.id());
        if (!obj)
            continue;

        obj->removeGroup();
        obj->deleteLater();
        groupObjects.remove(g.id());
    }
}

