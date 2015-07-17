/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#ifndef COMMHISTORY_GROUPOBJECT_H
#define COMMHISTORY_GROUPOBJECT_H

#include <QObject>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QDateTime>
#include <QDebug>

#include "event.h"
#include "group.h"
#include "libcommhistoryexport.h"

class QDBusArgument;

namespace CommHistory {

class GroupManager;
class GroupObjectPrivate;

/*!
 * \class GroupObject
 *
 * Conversation (or, group chat) data. Created by GroupManager, and updated
 * automatically with changes to the group locally or remotely.
 */
class LIBCOMMHISTORY_EXPORT GroupObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isValid READ isValid CONSTANT)
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString localUid READ localUid NOTIFY localUidChanged)
    Q_PROPERTY(RecipientList recipients READ recipients NOTIFY recipientsChanged);
    Q_PROPERTY(QStringList remoteUids READ remoteUids NOTIFY recipientsChanged);
    Q_PROPERTY(int chatType READ chatType NOTIFY chatTypeChanged)
    Q_PROPERTY(QString chatName READ chatName NOTIFY chatNameChanged)

    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged)
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY endTimeChanged)
    Q_PROPERTY(int unreadMessages READ unreadMessages NOTIFY unreadMessagesChanged)

    Q_PROPERTY(int lastEventId READ lastEventId NOTIFY lastEventIdChanged)
    Q_PROPERTY(QString lastMessageText READ lastMessageText NOTIFY lastMessageTextChanged)
    Q_PROPERTY(QString lastVCardFileName READ lastVCardFileName NOTIFY lastVCardFileNameChanged)
    Q_PROPERTY(QString lastVCardLabel READ lastVCardLabel NOTIFY lastVCardLabelChanged)
    Q_PROPERTY(int lastEventType READ lastEventType NOTIFY lastEventTypeChanged)
    Q_PROPERTY(int lastEventStatus READ lastEventStatus NOTIFY lastEventStatusChanged)
    Q_PROPERTY(bool lastEventIsDraft READ lastEventIsDraft NOTIFY lastEventIsDraftChanged)
    Q_PROPERTY(QDateTime lastModified READ lastModified NOTIFY lastModifiedChanged)

public:
    GroupObject(GroupManager *parent);
    GroupObject(const CommHistory::Group &other, GroupManager *parent = 0);
    virtual ~GroupObject();

    /*!
     * Mark all events as read
     */
    Q_INVOKABLE bool markAsRead();

    /*!
     * Delete the group and all its messages
     */
    Q_INVOKABLE bool deleteGroup();

    /*!
     * Object contains a valid group
     */
    bool isValid() const;

    /*!
     * Database id.
     */
    int id() const;
    void setId(int id);

    /*!
     * Tracker url of the group.
     */
    QUrl url() const;

    /*!
     * Local account uid.
     */
    QString localUid() const;
    void setLocalUid(const QString &uid);

    /*!
     * Remote recipients participating in this conversation.
     */
    RecipientList recipients() const;
    void setRecipients(const RecipientList &recipients);

    QStringList remoteUids() const { return recipients().remoteUids(); }

    /*!
     * Chat type (roughly corresponds to Telepathy handle type).
     * Default for new groups is Group::ChatTypeP2P.
     */
    Group::ChatType chatType() const;
    void setChatType(Group::ChatType chatType);

    /*!
     * Name for the chat room / group chat (optional).
     */
    QString chatName() const;
    void setChatName(const QString &name);

    /*!
     * Start time of the last message.
     * For incoming messages this is the time message was sent. For
     * outgoing messages this is the time message was delivered (or sent
     * if delivery reporting is not enabled).
     */
    QDateTime startTime() const;
    void setStartTime(const QDateTime &startTime);

    /*!
     * End time of the last message.
     * For incoming messages this is the time message was received.
     * For outgoing messages this is the time message was sent.
     */
    QDateTime endTime() const;
    void setEndTime(const QDateTime &endTime);

    /*!
     * Number of unread messages in this conversation.
     */
    int unreadMessages() const;
    void setUnreadMessages(int unread);

    /*!
     * Database id of the last message. -1 if the group has no messages.
     */
    int lastEventId() const;
    void setLastEventId(int id);

    /*!
     * Text of the last message.
     * This property is not stored in the database. It is filled in by
     * the model at runtime, if possible.
     */
    QString lastMessageText() const;
    void setLastMessageText(const QString &text);

    /*!
     * Filename of the vcard in the last event, if any.
     * Notice: this is not the last vcard in a group, but the vcard in the last message, which
     * might of course be empty.
     */
    QString lastVCardFileName() const;
    void setLastVCardFileName(const QString &filename);

    /*!
     * Label of the vcard in the last event, if any.
     * Notice: this is not the last vcard in a group, but the vcard in the last message, which
     * might of course be empty.
     */
    QString lastVCardLabel() const;
    void setLastVCardLabel(const QString &label);

    /*!
     * Type of the last sent / received message. @see Event::EventType
     */
    Event::EventType lastEventType() const;
    void setLastEventType(Event::EventType eventType);

    /*!
     * Status of last message, for received messages status is always equal
     * to Event::UnknownStatus
     */
    Event::EventStatus lastEventStatus() const;
    void setLastEventStatus(Event::EventStatus eventStatus);

    /*!
     * True if the last event is a draft message
     */
    bool lastEventIsDraft() const;
    void setLastEventIsDraft(bool isDraft);

    QDateTime lastModified() const;
    void setLastModified(const QDateTime &modified);

    quint32 startTimeT() const;
    void setStartTimeT(quint32 startTime);

    quint32 endTimeT() const;
    void setEndTimeT(quint32 endTime);

    quint32 lastModifiedT() const;
    void setLastModifiedT(quint32 modified);

    /* API from Group */
    static int urlToId(const QString &url);
    static QUrl idToUrl(int id);

    /*!
     * Get set of valid properties (i.e. properties that have been
     * assigned a value since the group was created) for this group.
     *
     * \return Set of valid properties.
     */
    Group::PropertySet validProperties() const;

    /*!
     * Set valid properties. API users should not normally need this, as
     * properties are automatically marked valid when modified.
     *
     * \param properties New set of properties.
     */
    void setValidProperties(const Group::PropertySet &properties);

    /*!
     * Get set of modified properties.
     *
     * \return Set of modified properties.
     */
    Group::PropertySet modifiedProperties() const;

    /*!
     * Reset modified properties. API users should not normally need this.
     *
     * \param properties New set of properties.
     */
    void resetModifiedProperties();

    QString toString() const;

    void set(const Group &other);
    void copyValidProperties(const Group &other);

    Group toGroup() const;

signals:
    void localUidChanged();
    void recipientsChanged();
    void chatTypeChanged();
    void chatNameChanged();
    void startTimeChanged();
    void endTimeChanged();
    void unreadMessagesChanged();
    void lastEventIdChanged();
    void lastMessageTextChanged();
    void lastVCardFileNameChanged();
    void lastVCardLabelChanged();
    void lastEventTypeChanged();
    void lastEventStatusChanged();
    void lastEventIsDraftChanged();
    void lastModifiedChanged();

    void groupDeleted();

private:
    friend class GroupManagerPrivate;
    friend class GroupObjectPrivate;
    GroupObjectPrivate *d;
};

}

Q_DECLARE_METATYPE(QList<CommHistory::GroupObject*>)
Q_DECLARE_METATYPE(CommHistory::GroupObject*)

#endif
