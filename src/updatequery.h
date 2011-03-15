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

#ifndef COMMHISTORY_UPDATE_QUERY_H
#define COMMHISTORY_UPDATE_QUERY_H

#include <QUrl>
#include <QStringList>
#include <QDateTime>

namespace CommHistory {

class UpdateQuery {
public:
    UpdateQuery();

    void deletion(const QUrl &subject,
                  const char *predicate,
                  const QString &object = QString());

    void resourceDeletion(const QUrl &subject,
                          const char *predicate);

    void deletion(const QString &query);

    void insertionRaw(const QUrl &subject,
                      const char *predicate,
                      const QString &object,
                      bool modify = false);

    void insertion(const QUrl &subject,
                   const char *predicate,
                   const QString &object,
                   bool modify = false);

    void insertion(const QUrl &subject,
                   const char *predicate,
                   const QDateTime &object,
                   bool modify = false);

    void insertion(const QUrl &subject,
                   const char *predicate,
                   bool object,
                   bool modify = false);

    void insertion(const QUrl &subject,
                   const char *predicate,
                   int object,
                   bool modify = false);

    void insertion(const QUrl &subject,
                   const char *predicate,
                   const QUrl &object,
                   bool modify = false);

    void insertion(const QString &statement);

    void insertionSilent(const QString &statement);

    // statement = raw content, not wrapped in INSERT {}
    void appendInsertion(const QString &statement);

    QString query();

private:
    QString nextVariable();

private:
    int nextVar;
    QStringList deletions;
    QMultiMap<QUrl, QString> insertions;
    QStringList postInsertions;
    QStringList silents;
};

} // namespace

#endif
