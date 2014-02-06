/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#ifndef COMMHISTORY_MESSAGEPART_H
#define COMMHISTORY_MESSAGEPART_H

#include <QSharedDataPointer>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QVariant>

#include "libcommhistoryexport.h"

class QDBusArgument;

namespace CommHistory {
    class MessagePart;
}

LIBCOMMHISTORY_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::MessagePart &part);
LIBCOMMHISTORY_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CommHistory::MessagePart &part);

LIBCOMMHISTORY_EXPORT QDataStream &operator<<(QDataStream &stream, const CommHistory::MessagePart &part);
LIBCOMMHISTORY_EXPORT QDataStream &operator>>(QDataStream &stream, CommHistory::MessagePart &part);

namespace CommHistory {

class MessagePartPrivate;

class LIBCOMMHISTORY_EXPORT MessagePart
{
public:
    MessagePart();
    MessagePart(const MessagePart &other);
    MessagePart &operator=(const MessagePart &other);
    bool operator==(const MessagePart &other) const;
    ~MessagePart();

    /*!
     * Unique ID of this message part on the system.
     */
    int id() const;
    void setId(int id);

    /*!
     * Identifier for this part within an event, set externally.
     *
     * For example, in a MMS message this is the content ID referenced
     * in SMIL.
     */
    QString contentId() const;
    void setContentId(const QString &id);

    /*!
     * Mime type of content
     */
    QString contentType() const;
    void setContentType(const QString &type);

    /*!
     * Filesystem path of the message part data
     */
    QString path() const;
    void setPath(const QString &path);

    /*!
     * Size on the filesystem of this message part
     */
    int size() const;

    /*!
     * Message part contents as plain text if possible, otherwise null.
     *
     * Null is returned when contentType doesn't start with text/
     */
    QString plainTextContent() const;

    QString debugString() const;

private:
    QSharedDataPointer<MessagePartPrivate> d;

    friend const QDBusArgument &::operator>>(const QDBusArgument &argument, CommHistory::MessagePart &part);
    friend QDataStream &::operator>>(QDataStream &stream, CommHistory::MessagePart &part);
};

}

Q_DECLARE_METATYPE(CommHistory::MessagePart);
Q_DECLARE_METATYPE(QList<CommHistory::MessagePart>);

#endif
