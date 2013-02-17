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

using namespace CommHistory;

ConversationProxyModel::ConversationProxyModel(QObject *parent)
    : ConversationModel(parent), m_contactGroup(0)
{
}

void ConversationProxyModel::setContactGroup(QObject *o)
{
    ContactGroup *g = qobject_cast<ContactGroup*>(o);
    if (m_contactGroup == g || (o && !g))
        return;

    if (m_contactGroup)
        disconnect(m_contactGroup, SIGNAL(groupsChanged()), this, SLOT(updateContactGroup()));

    m_contactGroup = g;

    if (m_contactGroup) {
        connect(m_contactGroup, SIGNAL(groupsChanged()), SLOT(updateContactGroup()));
        updateContactGroup();
    } else
        getEvents(QList<int>());
}

void ConversationProxyModel::updateContactGroup()
{
    if (!m_contactGroup)
        return;

    QList<GroupObject*> groups = m_contactGroup->groups();
    QList<int> groupIds;
    groupIds.reserve(groups.size());

    foreach (GroupObject *group, groups)
        groupIds.append(group->id());

    getEvents(groupIds);
}

