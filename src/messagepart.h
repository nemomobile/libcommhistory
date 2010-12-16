/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

class MessagePartPrivate;

class LIBCOMMHISTORY_EXPORT MessagePart
{
public:
    MessagePart();
    MessagePart(const MessagePart &other);
    MessagePart &operator=(const MessagePart &other);
    bool operator==(const MessagePart &other) const;
    ~MessagePart();

    // URI of the tracker node
    QString uri() const;

    QString contentId() const;

    QString plainTextContent() const;

    QString contentType() const;

    QString characterSet() const;

    int contentSize() const;

    QString contentLocation() const;


    void setUri(const QString &uri);

    void setContentId(const QString &id);

    void setPlainTextContent(const QString &text);

    void setContentType(const QString &type);

    void setCharacterSet(const QString &characterSet);

    void setContentSize(int size);

    void setContentLocation(const QString &location);


    QString toString() const;

private:
    QSharedDataPointer<MessagePartPrivate> d;
};

}

LIBCOMMHISTORY_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::MessagePart &part);
LIBCOMMHISTORY_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CommHistory::MessagePart &part);

Q_DECLARE_METATYPE(CommHistory::MessagePart);
Q_DECLARE_METATYPE(QList<CommHistory::MessagePart>);

#endif
