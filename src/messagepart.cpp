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

#include <QDebug>
#include <QSharedDataPointer>
#include <QDBusArgument>
#include <QStringBuilder>
#include "messagepart.h"

namespace CommHistory {

class MessagePartPrivate : public QSharedData
{
public:
    MessagePartPrivate();
    MessagePartPrivate(const MessagePartPrivate &other);
    ~MessagePartPrivate();

    QString uri;
    QString contentId;
    QString textContent;
    QString contentType;
    QString characterSet;
    int contentSize;
    QString contentLocation;
};

}

using namespace CommHistory;

QDBusArgument &operator<<(QDBusArgument &argument, const MessagePart &part)
{
    argument.beginStructure();
    argument << part.uri() << part.contentId() << part.plainTextContent()
             << part.contentType() << part.characterSet()
             << part.contentSize() << part.contentLocation();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MessagePart &part)
{
    MessagePartPrivate p;
    argument.beginStructure();
    argument >> p.uri >> p.contentId >> p.textContent
             >> p.contentType >> p.characterSet
             >> p.contentSize >> p.contentLocation;
    argument.endStructure();

    part.setUri(p.uri);
    part.setContentId(p.contentId);
    part.setPlainTextContent(p.textContent);
    part.setContentType(p.contentType);
    part.setCharacterSet(p.characterSet);
    part.setContentSize(p.contentSize);
    part.setContentLocation(p.contentLocation);

    return argument;
}

MessagePartPrivate::MessagePartPrivate() :
    uri(QString()),
    contentId(QString()),
    textContent(QString()),
    contentType(QString()),
    characterSet(QString()),
    contentSize(0),
    contentLocation(QString())
{
}

MessagePartPrivate::MessagePartPrivate(const MessagePartPrivate &other)
    : QSharedData(other), uri(other.uri), contentId(other.contentId),
      textContent(other.textContent), contentType(other.contentType),
      characterSet(other.characterSet), contentSize(other.contentSize),
      contentLocation(other.contentLocation)
{
}

MessagePartPrivate::~MessagePartPrivate()
{
}

MessagePart::MessagePart()
    : d(new MessagePartPrivate)
{
}

MessagePart::MessagePart(const MessagePart &other)
    : d(other.d)
{
}

MessagePart &MessagePart::operator=(const MessagePart &other)
{
    d = other.d;
    return *this;
}

bool MessagePart::operator==(const MessagePart &other) const
{
    return (this->d->contentId == other.contentId()
            && this->d->textContent == other.plainTextContent()
            && this->d->contentType == other.contentType()
            && this->d->characterSet == other.characterSet()
            && this->d->contentSize == other.contentSize()
            && this->d->contentLocation == other.contentLocation());
}

MessagePart::~MessagePart()
{
}

QString MessagePart::uri() const
{
    return d->uri;
}

QString MessagePart::contentId() const
{
    return d->contentId;
}

QString MessagePart::plainTextContent() const
{
    return d->textContent;
}

QString MessagePart::contentType() const
{
    return d->contentType;
}

QString MessagePart::characterSet() const
{
    return d->characterSet;
}

int MessagePart::contentSize() const
{
    return d->contentSize;
}

QString MessagePart::contentLocation() const
{
    return d->contentLocation;
}


void MessagePart::setUri(const QString &uri)
{
    d->uri = uri;
}

void MessagePart::setContentId(const QString &id)
{
    d->contentId = id;
}

void MessagePart::setPlainTextContent(const QString &text)
{
    d->textContent = text;
}

void MessagePart::setContentType(const QString &type)
{
    d->contentType = type;
}

void MessagePart::setCharacterSet(const QString &characterSet)
{
    d->characterSet = characterSet;
}

void MessagePart::setContentSize(int size)
{
    d->contentSize = size;
}

void MessagePart::setContentLocation(const QString &location)
{
    d->contentLocation = location;
}

QString MessagePart::toString() const
{
    return QString(contentId()                    % QChar('|') %
                   contentType()                  % QChar('|') %
                   characterSet()                 % QChar('|') %
                   QString::number(contentSize()) % QChar('|') %
                   contentLocation()              % QChar('|') %
                   plainTextContent());
}
