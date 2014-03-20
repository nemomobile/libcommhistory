/* Copyright (C) 2014 Jolla Ltd.
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

#include "mmshelper.h"
#include "singleeventmodel.h"
#include <QtDBus>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QTextCodec>
#include <QTemporaryFile>
#include <QDebug>

// MmsEngine, implemented by the backend mms-engine
#define MMS_ENGINE_SERVICE "org.nemomobile.MmsEngine"
#define MMS_ENGINE_PATH "/"
#define MMS_ENGINE_INTERFACE "org.nemomobile.MmsEngine"

// MmsHandler, implemented by commhistory-daemon
#define MMS_HANDLER_SERVICE "org.nemomobile.MmsHandler"
#define MMS_HANDLER_PATH "/"
#define MMS_HANDLER_INTERFACE "org.nemomobile.MmsHandler"

struct MmsPart
{
    QString fileName;
    QString contentType;
    QString contentId;
};

inline QDBusArgument& operator<<(QDBusArgument &arg, const MmsPart &part)
{
    arg.beginStructure();
    arg << part.fileName << part.contentType << part.contentId;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument& operator>>(const QDBusArgument &arg, MmsPart &part)
{
    arg.beginStructure();
    arg >> part.fileName >> part.contentType >> part.contentId;
    arg.endStructure();
    return arg;
}

typedef QList<MmsPart> MmsPartList;

Q_DECLARE_METATYPE(MmsPart);
Q_DECLARE_METATYPE(MmsPartList);

using namespace CommHistory;

MmsHelper::MmsHelper(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<MmsPart>();
    qDBusRegisterMetaType<MmsPartList>();
}

bool MmsHelper::receiveMessage(int id)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(id))
        event = model.event(model.index(0, 0));

    if (!event.isValid()) {
        qWarning() << "MmsHelper::receiveMessage called for unknown event id" << id;
        return false;
    }

    QString imsi = event.extraProperty("mms-notification-imsi").toString();
    QByteArray pushData = QByteArray::fromBase64(event.extraProperty("mms-push-data").toByteArray());

    if (imsi.isEmpty() || pushData.isEmpty()) {
        qWarning() << "MmsHelper::receivedMessage called for event" << id << "without notification data";
        event.setStatus(Event::PermanentlyFailedStatus);
        model.modifyEvent(event);
        return false;
    }

    event.setStatus(Event::WaitingStatus);
    model.modifyEvent(event);

    QVariantList args;
    args << id << imsi << true << pushData;
    QDBusMessage method = QDBusMessage::createMethodCall(MMS_ENGINE_SERVICE, MMS_ENGINE_PATH, MMS_ENGINE_INTERFACE, "receiveMessage");
    method.setArguments(args);
    QDBusPendingCall call = QDBusConnection::systemBus().asyncCall(method);
    return true;
}

bool MmsHelper::cancel(int id)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(id))
        event = model.event(model.index(0, 0));

    if (!event.isValid()) {
        qWarning() << "MmsHelper::cancel called for unknown event id" << id;
        return false;
    }

    if (event.status() != Event::DownloadingStatus && event.status() != Event::WaitingStatus && event.status() != Event::SendingStatus)
        return false;

    QVariantList args;
    args << id;
    QDBusMessage method = QDBusMessage::createMethodCall(MMS_ENGINE_SERVICE, MMS_ENGINE_PATH, MMS_ENGINE_INTERFACE, "cancel");
    method.setArguments(args);
    QDBusConnection::systemBus().asyncCall(method);

    if (event.direction() == Event::Inbound)
        event.setStatus(Event::ManualNotificationStatus);
    else
        event.setStatus(Event::TemporarilyFailedStatus);
    return model.modifyEvent(event);
}

bool MmsHelper::sendReadReport(int id)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(id))
        event = model.event(model.index(0, 0));

    if (!event.isValid()) {
        qWarning() << "MmsHelper::sendReadReport called for unknown event id" << id;
        return false;
    }

    QString imsi = event.extraProperty("mms-notification-imsi").toString();
    if (imsi.isEmpty() || event.mmsId().isEmpty()) {
        qWarning() << "MmsHelper::sendReadReport called for event" << id << "without MMS data";
        return false;
    }

    QVariantList args;
    args << id << imsi << event.mmsId() << event.remoteUid() << 0;
    QDBusMessage method = QDBusMessage::createMethodCall(MMS_ENGINE_SERVICE, MMS_ENGINE_PATH, MMS_ENGINE_INTERFACE, "sendReadReport");
    method.setArguments(args);
    QDBusConnection::systemBus().asyncCall(method);
    return true;
}

static QString createTemporaryTextFile(const QString &text, QString &contentType)
{
    QString codec(QStringLiteral("utf-8"));
    QByteArray data = text.toUtf8();

    if (contentType.isEmpty())
        contentType = QStringLiteral("text/plain");
    else
        contentType = contentType.left(contentType.indexOf(';'));
    contentType += QStringLiteral(";charset=") + codec;

    QTemporaryFile file;
    if (!file.open())
        return QString();

    if (file.write(data) < data.size())
        return QString();

    file.setAutoRemove(false);
    return file.fileName();
}

/* Parts should be an array of objects, each with the following:
 *   - contentId = string/integer uniquely representing the part within this message)
 *   - contentType = mime type, include charset for text
 *   - path = path to a file, which will be copied for the event
 *   - freeText = Instead of path, string contents for a textual part
 */
bool MmsHelper::sendMessage(const QStringList &to, const QStringList &cc, const QStringList &bcc, const QString &subject, const QVariantList &parts)
{
    MmsPartList outParts;
    foreach (const QVariant &v, parts) {
        QVariantMap p = v.toMap();
        MmsPart part;
        part.contentId = p["contentId"].toString();
        part.contentType = p["contentType"].toString();
        if (part.contentId.isEmpty() || part.contentType.isEmpty())
            return false;

        part.fileName = p["path"].toString();
        if (part.fileName.isEmpty()) {
            QString freeText = p["freeText"].toString();
            if (!freeText.isEmpty())
                part.fileName = createTemporaryTextFile(freeText, part.contentType);
            if (part.fileName.isEmpty())
                return false;
        } else if (part.fileName.startsWith("file://"))
            part.fileName = QUrl(part.fileName).toLocalFile();

        outParts.append(part);
    }

    QVariantList args;
    args << to << cc << bcc << subject << QVariant::fromValue(outParts);
    QDBusMessage method = QDBusMessage::createMethodCall(MMS_HANDLER_SERVICE, MMS_HANDLER_PATH, MMS_HANDLER_INTERFACE, "sendMessage");
    method.setArguments(args);
    QDBusConnection::systemBus().asyncCall(method);
    return true;
}

bool MmsHelper::retrySendMessage(int eventId)
{
    QDBusMessage method = QDBusMessage::createMethodCall(MMS_HANDLER_SERVICE, MMS_HANDLER_PATH, MMS_HANDLER_INTERFACE, "sendMessageFromEvent");
    method.setArguments(QVariantList() << eventId);
    QDBusConnection::systemBus().asyncCall(method);
    return true;
}

