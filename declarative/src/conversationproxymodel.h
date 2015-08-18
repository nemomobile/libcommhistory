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

#ifndef COMMHISTORY_DECLARATIVE_CONVERSATIONPROXYMODEL_H
#define COMMHISTORY_DECLARATIVE_CONVERSATIONPROXYMODEL_H

#include <QIdentityProxyModel>
#include <QHash>
#include "conversationmodel.h"
#include "sharedbackgroundthread.h"
#include "contactgroup.h"

class ConversationProxyModel : public CommHistory::ConversationModel
{
    Q_OBJECT

public:
    ConversationProxyModel(QObject *parent = 0);

    Q_PROPERTY(QObject* contactGroup READ contactGroup WRITE setContactGroup NOTIFY contactGroupChanged)
    CommHistory::ContactGroup *contactGroup() const { return m_contactGroup; }
    void setContactGroup(QObject *group);

    Q_PROPERTY(bool useBackgroundThread READ useBackgroundThread WRITE setUseBackgroundThread NOTIFY backgroundThreadChanged)
    bool useBackgroundThread() { return backgroundThread() != 0; }
    void setUseBackgroundThread(bool on);

    Q_PROPERTY(int groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    int groupId() const { return m_groupId; }
    void setGroupId(int groupId);

    Q_PROPERTY(bool resolveContacts READ resolveContacts WRITE setResolveContacts NOTIFY resolveContactsChanged)
    // Shadow ConversationModel functions:
    bool resolveContacts() const;
    void setResolveContacts(bool enabled);

public slots:
    void reload();

signals:
    void contactGroupChanged();
    void groupIdChanged();
    void backgroundThreadChanged();
    void resolveContactsChanged();

private:
    CommHistory::ContactGroup *m_contactGroup;
    int m_groupId;
    QSharedPointer<QThread> threadInstance;
};

#endif
