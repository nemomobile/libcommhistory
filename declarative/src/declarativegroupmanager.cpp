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

#include "declarativegroupmanager.h"
#include "sharedbackgroundthread.h"
#include "singleeventmodel.h"
#include <QTimer>
#include "debug.h"

using namespace CommHistory;

DeclarativeGroupManager::DeclarativeGroupManager(QObject *parent)
    : CommHistory::GroupManager(parent)
{
    QTimer::singleShot(0, this, SLOT(reload()));
}

DeclarativeGroupManager::~DeclarativeGroupManager()
{
}

void DeclarativeGroupManager::reload()
{
    getGroups();
}

void DeclarativeGroupManager::setUseBackgroundThread(bool enabled)
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

int DeclarativeGroupManager::createOutgoingMessageEvent(int groupId, const QString &localUid,
                                                        const QString &remoteUid, const QString &text)
{
    return createOutgoingMessageEvent(groupId, localUid, QStringList() << remoteUid, text);
}

int DeclarativeGroupManager::createOutgoingMessageEvent(int groupId, const QString &localUid,
                                                        const QStringList &remoteUids, const QString &text)
{
    if (groupId < 0)
        groupId = ensureGroupExists(localUid, remoteUids);

    if (groupId < 0)
        return -1;

    Event event;
    if (localUid.indexOf("/ring/tel/") >= 0)
        event.setType(Event::SMSEvent);
    else
        event.setType(Event::IMEvent);

    event.setDirection(Event::Outbound);
    event.setIsRead(true);
    event.setLocalUid(localUid);
    event.setRecipients(RecipientList::fromUids(localUid, remoteUids));
    event.setFreeText(text);
    event.setStartTimeT(Event::currentTime_t());
    event.setEndTimeT(event.startTimeT());
    event.setStatus(Event::SendingStatus);
    event.setGroupId(groupId);

    DEBUG() << Q_FUNC_INFO << groupId << localUid << remoteUids << text;
    EventModel model;
    if (model.addEvent(event))
        return event.id();

    qWarning() << Q_FUNC_INFO << "Failed creating event";
    return -1;
}

bool DeclarativeGroupManager::setEventStatus(int eventId, int status)
{
    SingleEventModel model;
    if (!model.getEventById(eventId)) {
        qWarning() << Q_FUNC_INFO << "No event with id" << eventId;
        return false;
    }

    Event ev = model.event();
    if (ev.status() == status)
        return true;

    ev.setStatus(static_cast<Event::EventStatus>(status));
    return model.modifyEvent(ev);
}

int DeclarativeGroupManager::ensureGroupExists(const QString &localUid, const QStringList &remoteUids)
{
    // Try to find an appropriate group
    GroupObject *group = findGroup(localUid, remoteUids);
    if (group) {
        return group->id();
    } else {
        Group g;
        g.setLocalUid(localUid);
        g.setRecipients(RecipientList::fromUids(localUid, remoteUids));
        g.setChatType(Group::ChatTypeP2P);
        DEBUG() << Q_FUNC_INFO << "Creating group for" << localUid << remoteUids;
        if (!addGroup(g)) {
            qWarning() << Q_FUNC_INFO << "Failed creating group";
            return -1;
        }
        return g.id();
    }
}

