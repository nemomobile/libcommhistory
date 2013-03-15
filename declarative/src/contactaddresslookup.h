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

#ifndef CONTACTADDRESSLOOKUP_H
#define CONTACTADDRESSLOOKUP_H

#include <QObject>
#include <QSharedPointer>
#include "contactlistener.h"

class ContactAddressLookup : public QObject
{
    Q_OBJECT

public:
    ContactAddressLookup(QObject *parent = 0);

    Q_PROPERTY(QString localUid READ localUid WRITE setLocalUid NOTIFY localUidChanged)
    QString localUid() const { return mLocalUid; }
    void setLocalUid(const QString &localUid);

    Q_PROPERTY(QString remoteUid READ remoteUid WRITE setRemoteUid NOTIFY remoteUidChanged)
    QString remoteUid() const { return mRemoteUid; }
    void setRemoteUid(const QString &remoteUid);

    Q_PROPERTY(int contactId READ contactId NOTIFY contactIdChanged)
    int contactId() const { return mContactId; }

signals:
    void localUidChanged();
    void remoteUidChanged();
    void contactIdChanged();

private slots:
    void request();
    void contactUpdated(quint32 id, const QString &name, const QList<QPair<QString, QString> > &addresses);

private:
    QString mLocalUid, mRemoteUid;
    int mContactId;
    bool requestPending;
    QSharedPointer<CommHistory::ContactListener> listener;
};

#endif
