/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013-2014 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#ifndef COMMHISTORYDATABASE_H
#define COMMHISTORYDATABASE_H

#include <QSqlDatabase>
#include "libcommhistoryexport.h"

class CommHistoryDatabase
{
public:
    static QSqlDatabase open(const QString &databaseName);
    static QSqlQuery prepare(const char *statement, const QSqlDatabase &database);
    static LIBCOMMHISTORY_EXPORT QString dataDir(int id);
};

#endif
