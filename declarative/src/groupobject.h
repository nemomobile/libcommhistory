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

#ifndef COMMHISTORY_DECLARATIVE_GROUP_H
#define COMMHISTORY_DECLARATIVE_GROUP_H

#include <QObject>
#include "group.h"

class GroupProxyModel;
class QModelIndex;

class GroupObject : public QObject, CommHistory::Group
{
    Q_OBJECT
    Q_ENUMS(ChatType)

public:
    GroupObject(QObject *parent = 0);
    GroupObject(const CommHistory::Group &group, GroupProxyModel *parent = 0);
    virtual ~GroupObject();

    Q_PROPERTY(bool isValid READ isValid CONSTANT);
    Q_PROPERTY(int id READ id CONSTANT);
    Q_PROPERTY(QString localUid READ localUid NOTIFY localUidChanged);
    Q_PROPERTY(QStringList remoteUids READ remoteUids NOTIFY remoteUidsChanged);
    Q_PROPERTY(int chatType READ chatType NOTIFY chatTypeChanged);
    Q_PROPERTY(QString chatName READ chatName NOTIFY chatNameChanged);
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged);
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY endTimeChanged);
    Q_PROPERTY(int totalMessages READ totalMessages NOTIFY totalMessagesChanged);
    Q_PROPERTY(int unreadMessages READ unreadMessages NOTIFY unreadMessagesChanged);
    Q_PROPERTY(int sentMessages READ sentMessages NOTIFY sentMessagesChanged);
    Q_PROPERTY(int lastEventId READ lastEventId NOTIFY lastEventIdChanged);
    Q_PROPERTY(int contactId READ contactId NOTIFY contactsChanged);
    Q_PROPERTY(QString contactName READ contactName NOTIFY contactsChanged);
    Q_PROPERTY(QString lastMessageText READ lastMessageText NOTIFY lastMessageTextChanged);
    Q_PROPERTY(QString lastVCardFileName READ lastVCardFileName NOTIFY lastVCardFileNameChanged);
    Q_PROPERTY(QString lastVCardLabel READ lastVCardLabel NOTIFY lastVCardLabelChanged);
    Q_PROPERTY(int lastEventType READ lastEventType NOTIFY lastEventTypeChanged);
    Q_PROPERTY(int lastEventStatus READ lastEventStatus NOTIFY lastEventStatusChanged);
    Q_PROPERTY(QDateTime lastModified READ lastModified NOTIFY lastModifiedChanged);

    using Group::isValid;
    using Group::id;
    using Group::localUid;
    using Group::remoteUids;
    int chatType() const { return static_cast<int>(Group::chatType()); }
    using Group::chatName;
    using Group::startTime;
    using Group::endTime;
    using Group::totalMessages;
    using Group::unreadMessages;
    using Group::sentMessages;
    using Group::contacts;
    using Group::lastEventId;
    using Group::lastMessageText;
    using Group::lastVCardFileName;
    using Group::lastVCardLabel;
    int lastEventType() const { return static_cast<int>(Group::lastEventType()); }
    int lastEventStatus() const { return static_cast<int>(Group::lastEventStatus()); }
    using Group::lastModified;

    using Group::contactId;
    using Group::contactName;

    void updateGroup(const CommHistory::Group &group);
    void removeGroup();

signals:
    void localUidChanged();
    void remoteUidsChanged();
    void chatTypeChanged();
    void chatNameChanged();
    void startTimeChanged();
    void endTimeChanged();
    void totalMessagesChanged();
    void unreadMessagesChanged();
    void sentMessagesChanged();
    void lastEventIdChanged();
    void contactsChanged();
    void lastMessageTextChanged();
    void lastVCardFileNameChanged();
    void lastVCardLabelChanged();
    void lastEventTypeChanged();
    void lastEventStatusChanged();
    void lastModifiedChanged();

    void groupRemoved();

private:
    GroupProxyModel *model;
};

#endif

