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

#ifndef COMMHISTORY_DECLARATIVE_DECLARATIVEGROUPMANAGER_H
#define COMMHISTORY_DECLARATIVE_DECLARATIVEGROUPMANAGER_H

#include "groupmanager.h"
#include "sharedbackgroundthread.h"

#include <QJSValue>

class DeclarativeGroupManager : public CommHistory::GroupManager
{
    Q_OBJECT

    Q_PROPERTY(bool useBackgroundThread READ useBackgroundThread WRITE setUseBackgroundThread NOTIFY backgroundThreadChanged)
    Q_PROPERTY(bool resolveContacts READ resolveContacts WRITE setResolveContacts NOTIFY resolveContactsChanged)

public:
    DeclarativeGroupManager(QObject *parent = 0);
    virtual ~DeclarativeGroupManager();

    bool useBackgroundThread() { return backgroundThread() != 0; }
    void setUseBackgroundThread(bool on);

    // Shadow GroupManager functions:
    bool resolveContacts() const;
    void setResolveContacts(bool enabled);

    /* Create an event for an outgoing plain text message, which will be
     * in the sending state. Returns the event ID, which should be passed
     * using the x-commhistory-event-id header in a Telepathy message to
     * inform commhistory-daemon about the existing event.
     *
     * If groupId is negative, an appropriate group will be found or created
     * inline if necessary. */
    Q_INVOKABLE int createOutgoingMessageEvent(int groupId, const QString &localUid, const QString &remoteUid, const QString &text);
    Q_INVOKABLE int createOutgoingMessageEvent(int groupId, const QString &localUid, const QStringList &remoteUids, const QString &text);

    /* Create an event for an outgoing plain text message, which will be
     * in the sending state. The event ID will be passed to the callback
     * function supplied, and should be passed using the x-commhistory-event-id
     * header in a Telepathy message to inform commhistory-daemon about the
     * existing event.
     *
     * If groupId is negative, an appropriate group will be found or created
     * inline if necessary. */
    Q_INVOKABLE void createOutgoingMessageEvent(int groupId, const QString &localUid, const QString &remoteUid, const QString &text, QJSValue callback);
    Q_INVOKABLE void createOutgoingMessageEvent(int groupId, const QString &localUid, const QStringList &remoteUids, const QString &text, QJSValue callback);

    Q_INVOKABLE bool setEventStatus(int eventId, int status);

    Q_INVOKABLE int ensureGroupExists(const QString &localUid, const QStringList &remoteUids);

public slots:
    void reload();

private slots:
    void eventWritten(int eventId, QJSValue callback);

signals:
    void backgroundThreadChanged();
    void resolveContactsChanged();

private:
    QSharedPointer<QThread> threadInstance;
};

#endif

