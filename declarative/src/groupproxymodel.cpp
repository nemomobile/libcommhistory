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

    model = g;
    QIdentityProxyModel::setSourceModel(model);

    QHash<int,QByteArray> roles = model->roleNames();
    roles[WeekdaySectionRole] = "weekdaySection";
    setRoleNames(roles);

    emit sourceModelChanged();
}

QVariant GroupProxyModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid())
        return QVariant();

    if (role == WeekdaySectionRole) {
        Group g = model->group(mapToSource(idx));
        QDateTime dateTime = g.endTime().toLocalTime();

        // Return the date for the past week, and group all older items together under an
        // arbitrary older date
        const int daysDiff = QDate::currentDate().toJulianDay() - dateTime.date().toJulianDay();
        if (daysDiff < 7)
            return dateTime.date();

        // Arbitrary static date for older items..
        return QDate(2000, 1, 1);
    }

    return QIdentityProxyModel::data(idx, role);
}

GroupObject *GroupProxyModel::group(int row)
{
    if (!model) {
        qWarning() << Q_FUNC_INFO << "No group model instance";
        return 0;
    }

    return model->groupObject(mapToSource(index(row, 0)));
}

GroupObject *GroupProxyModel::groupById(int id)
{
    if (!model) {
        qWarning() << Q_FUNC_INFO << "No group model instance";
        return 0;
    }

    for (int r = 0; r < model->rowCount(); r++) {
        GroupObject *g = model->groupObject(model->index(r, 0));
        if (!g || g->id() != id)
            continue;

        return g;
    }

    return 0;
}

