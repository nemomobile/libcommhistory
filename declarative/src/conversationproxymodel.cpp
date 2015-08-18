/* Copyright (C) 2013 Jolla Ltd.
 * Contact: John Brooks <john.brooks@jollamobile.com>
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

#include "conversationproxymodel.h"
#include "sharedbackgroundthread.h"
#include <QTimer>

using namespace CommHistory;

ConversationProxyModel::ConversationProxyModel(QObject *parent)
    : ConversationModel(parent), m_contactGroup(0), m_groupId(-1) 
{
    // Defaults
    setQueryMode(EventModel::StreamedAsyncQuery);
    setFirstChunkSize(25);
    setChunkSize(50);
    setTreeMode(false);
}

void ConversationProxyModel::setUseBackgroundThread(bool enabled)
{
    if (enabled == useBackgroundThread())
        return;

    if (enabled) {
        threadInstance = getSharedBackgroundThread();
        setBackgroundThread(threadInstance.data());
    } else {
        setBackgroundThread(0);
        threadInstance.clear();
    }

    emit backgroundThreadChanged();
}

void ConversationProxyModel::setContactGroup(QObject *o)
{
    ContactGroup *g = qobject_cast<ContactGroup*>(o);
    if (m_contactGroup == g || (o && !g))
        return;

    if (m_contactGroup)
        disconnect(m_contactGroup, SIGNAL(groupsChanged()), this, SLOT(reload()));

    m_contactGroup = g;
    emit contactGroupChanged();

    if (m_contactGroup && m_groupId >= 0) {
        m_groupId = -1;
        emit groupIdChanged();
    }

    if (m_contactGroup)
        connect(m_contactGroup, SIGNAL(groupsChanged()), SLOT(reload()));

    QTimer::singleShot(0, this, SLOT(reload()));
}

void ConversationProxyModel::setGroupId(int g)
{
    if (g == m_groupId)
        return;

    m_groupId = g;
    emit groupIdChanged();

    if (m_contactGroup)
        setContactGroup(0);
    else
        QTimer::singleShot(0, this, SLOT(reload()));
}

bool ConversationProxyModel::resolveContacts() const
{
    return (ConversationModel::resolveContacts() == EventModel::ResolveImmediately);
}

void ConversationProxyModel::setResolveContacts(bool enabled)
{
    if (resolveContacts() == enabled)
        return;

    ConversationModel::setResolveContacts(enabled ? EventModel::ResolveImmediately : EventModel::ResolveOnDemand);
    if (enabled)
        QTimer::singleShot(0, this, SLOT(reload()));
    emit resolveContactsChanged();
}

void ConversationProxyModel::reload()
{
    if (m_groupId >= 0) {
        getEvents(m_groupId);
    } else if (m_contactGroup) {
        QList<GroupObject*> groups = m_contactGroup->groups();
        QList<int> groupIds;
        groupIds.reserve(groups.size());

        foreach (GroupObject *group, groups)
            groupIds.append(group->id());

        getEvents(groupIds);
    } else {
        getEvents(QList<int>());
    }
}

