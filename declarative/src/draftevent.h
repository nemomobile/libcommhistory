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

#ifndef COMMHISTORY_DECLARATIVE_DRAFTEVENT_H
#define COMMHISTORY_DECLARATIVE_DRAFTEVENT_H

#include <QObject>
#include <QStringList>
#include "event.h"

class DraftEvent : public QObject
{
    Q_OBJECT

public:
    DraftEvent(QObject *parent = 0);
    ~DraftEvent();

    Q_PROPERTY(CommHistory::Event event READ event WRITE setEvent RESET reset NOTIFY eventChanged)
    CommHistory::Event event() const;
    void setEvent(const CommHistory::Event &event);

    Q_PROPERTY(int eventId READ eventId WRITE setEventId NOTIFY eventIdChanged)
    int eventId() const;
    void setEventId(int id);

    Q_PROPERTY(int groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    int groupId() const;
    void setGroupId(int id);

    Q_PROPERTY(QString localUid READ localUid WRITE setLocalUid NOTIFY localUidChanged)
    QString localUid() const;
    void setLocalUid(const QString &localUid);

    Q_PROPERTY(QStringList remoteUids READ remoteUids WRITE setRemoteUids NOTIFY remoteUidsChanged)
    QStringList remoteUids() const;
    void setRemoteUids(const QStringList &remoteUids);

    Q_PROPERTY(QString freeText READ freeText WRITE setFreeText NOTIFY freeTextChanged)
    QString freeText() const;
    void setFreeText(const QString &freeText);

    Q_PROPERTY(bool isModified READ isModified NOTIFY isModifiedChanged)
    bool isModified() const;

    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    bool isValid() const;

public slots:
    void save();
    void reset();
    void deleteEvent();

signals:
    void eventChanged();
    void eventIdChanged();
    void groupIdChanged();
    void localUidChanged();
    void remoteUidsChanged();
    void freeTextChanged();
    void isModifiedChanged();
    void isValidChanged();

private:
    CommHistory::Event m_event;
};

#endif
