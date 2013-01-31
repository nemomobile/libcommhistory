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

#ifndef COMMHISTORY_DECLARATIVE_GROUPPROXYMODEL_H
#define COMMHISTORY_DECLARATIVE_GROUPPROXYMODEL_H

#include <QIdentityProxyModel>
#include <QHash>

class GroupObject;

namespace CommHistory {
    class GroupModel;
}

class GroupProxyModel : public QIdentityProxyModel
{
    Q_OBJECT

public:
    enum {
        WeekdaySectionRole = Qt::UserRole + 2000
    };

    GroupProxyModel(QObject *parent = 0);

    Q_PROPERTY(QObject* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)
    virtual void setSourceModel(QAbstractItemModel *sourceModel);
    void setSourceModel(QObject *m)
    {
        setSourceModel(qobject_cast<QAbstractItemModel*>(m));
    }

    CommHistory::GroupModel *groupModel() const { return model; }

    Q_INVOKABLE GroupObject *group(int row);
    Q_INVOKABLE GroupObject *groupById(int id);

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

signals:
    void sourceModelChanged();

private slots:
    void sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void sourceRowsRemoved(const QModelIndex &parent, int start, int end);
    void sourceRowsMoved();

private:
    CommHistory::GroupModel *model;
    QHash<int,GroupObject*> groupObjects;
};

#endif
