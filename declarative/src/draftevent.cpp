/* Copyright (C) 2014 Jolla Ltd.
 * Contact: John Brooks <john.brooks@jolla.com>
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

#include "draftevent.h"
#include "singleeventmodel.h"
#include "commonutils.h"
#include <QDebug>

using namespace CommHistory;

DraftEvent::DraftEvent(QObject *parent)
    : QObject(parent)
{
    connect(this, SIGNAL(eventChanged()), SIGNAL(eventIdChanged()));
    connect(this, SIGNAL(eventChanged()), SIGNAL(groupIdChanged()));
    connect(this, SIGNAL(eventChanged()), SIGNAL(localUidChanged()));
    connect(this, SIGNAL(eventChanged()), SIGNAL(remoteUidsChanged()));
    connect(this, SIGNAL(eventChanged()), SIGNAL(freeTextChanged()));
    connect(this, SIGNAL(eventChanged()), SIGNAL(isModifiedChanged()));

    connect(this, SIGNAL(isModifiedChanged()), SIGNAL(isValidChanged()));
}

DraftEvent::~DraftEvent()
{
}

void DraftEvent::reset()
{
    setEvent(Event());
}

void DraftEvent::save()
{
    if (!isValid()) {
        qWarning() << "DraftEvent cannot save invalid event:" << m_event.toString();
        return;
    }

    if (!isModified())
        return;

    SingleEventModel model;
    m_event.setIsDraft(true);
    m_event.setIsRead(true);
    m_event.setStartTime(QDateTime::currentDateTime());
    m_event.setEndTime(QDateTime::currentDateTime());
    m_event.setType(localUidComparesPhoneNumbers(m_event.localUid()) ? Event::SMSEvent : Event::IMEvent);
    m_event.setDirection(Event::Outbound);

    if (m_event.id() < 0) {
        if (!model.addEvent(m_event))
            qWarning() << "DraftEvent add failed:" << m_event.toString();
    } else {
        if (!model.modifyEvent(m_event))
            qWarning() << "DraftEvent modify failed:" << m_event.toString();
    }
}

void DraftEvent::deleteEvent()
{
    SingleEventModel model;
    if (m_event.id() >= 0 && !model.deleteEvent(m_event))
        qWarning() << "DraftEvent delete failed:" << m_event.toString();
}

Event DraftEvent::event() const
{
    return m_event;
}

void DraftEvent::setEvent(const Event &event)
{
    if (event == m_event)
        return;

    m_event = event;
    emit eventChanged();
}

int DraftEvent::eventId() const
{
    return m_event.id();
}

void DraftEvent::setEventId(int id)
{
    if (id == m_event.id())
        return;
    m_event.setId(id);
    emit eventIdChanged();
    emit isModifiedChanged();
}

int DraftEvent::groupId() const
{
    return m_event.groupId();
}

void DraftEvent::setGroupId(int id)
{
    if (id == m_event.groupId())
        return;
    m_event.setGroupId(id);
    emit groupIdChanged();
    emit isModifiedChanged();
}

QString DraftEvent::localUid() const
{
    return m_event.localUid();
}

void DraftEvent::setLocalUid(const QString &localUid)
{
    if (localUid == m_event.localUid())
        return;
    m_event.setLocalUid(localUid);
    emit localUidChanged();
    emit isModifiedChanged();
}

QStringList DraftEvent::remoteUids() const
{
    return m_event.recipients().remoteUids();
}

void DraftEvent::setRemoteUids(const QStringList &remoteUids)
{
    if (remoteUids == m_event.recipients().remoteUids())
        return;

    if (m_event.localUid().isEmpty()) {
        qWarning() << "DraftEvent cannot set remote UIDs without a local UID";
        return;
    }

    m_event.setRecipients(RecipientList::fromUids(m_event.localUid(), remoteUids));
    emit remoteUidsChanged();
    emit isModifiedChanged();
}

QString DraftEvent::freeText() const
{
    return m_event.freeText();
}

void DraftEvent::setFreeText(const QString &freeText)
{
    if (freeText == m_event.freeText())
        return;
    m_event.setFreeText(freeText);
    emit freeTextChanged();
    emit isModifiedChanged();
}

bool DraftEvent::isModified() const
{
    return !m_event.modifiedProperties().isEmpty();
}

bool DraftEvent::isValid() const
{
    return !m_event.localUid().isEmpty() &&
           !m_event.recipients().isEmpty() &&
           !m_event.freeText().isEmpty() &&
           m_event.groupId() >= 0;
}

