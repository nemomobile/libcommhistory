/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
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

#include "commhistorydatabase.h"
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QDesktopServices>
#else
#include <QStandardPaths>
#endif

// Appended to GenericDataLocation (or a hardcoded equivalent on Qt4)
#define COMMHISTORY_DATABASE_DIR "/commhistory/"
#define COMMHISTORY_DATABASE_NAME "commhistory.db"

static const char *db_setup[] = {
    "PRAGMA temp_store = MEMORY",
    "PRAGMA journal_mode = WAL",
    "PRAGMA foreign_keys = ON"
};
static int db_setup_count = sizeof(db_setup) / sizeof(*db_setup);

static const char *db_schema[] = {
    "PRAGMA encoding = \"UTF-16\"",

    "CREATE TABLE Groups ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  localUid TEXT, "
    "  remoteUids TEXT, "
    "  type INTEGER, "
    "  chatName TEXT, "
    "  lastModified INTEGER UNSIGNED "
    ")",

    "CREATE TABLE Events ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  type INTEGER, "
    "  startTime INTEGER, "
    "  endTime INTEGER, "
    "  direction INTEGER, "
    "  isDraft INTEGER, "
    "  isRead INTEGER, "
    "  isMissedCall INTEGER, "
    "  isEmergencyCall INTEGER, "
    "  status INTEGER, "
    "  bytesReceived INTEGER, "
    "  localUid TEXT, "
    "  remoteUid TEXT, "
    "  parentId INTEGER, "
    "  subject TEXT, "
    "  freeText TEXT, "
    "  groupId INTEGER, "
    "  messageToken TEXT, "
    "  lastModified INTEGER, "
    "  vCardFileName TEXT, "
    "  vCardLabel TEXT, "
    "  isDeleted INTEGER, "
    "  reportDelivery INTEGER, "
    "  validityPeriod INTEGER, "
    "  contentLocation TEXT, "
    "  messageParts TEXT, "
    "  headers TEXT, "
    "  readStatus INTEGER, "
    "  reportRead INTEGER, "
    "  reportedReadRequested INTEGER, "
    "  mmsId INTEGER, "
    "  isAction INTEGER, "
    "  FOREIGN KEY(groupId) REFERENCES Groups(id) ON DELETE CASCADE "
    ")"
};
static int db_schema_count = sizeof(db_schema) / sizeof(*db_schema);

static bool execute(QSqlDatabase &database, const QString &statement)
{
    QSqlQuery query(database);
    if (!query.exec(statement)) {
        qWarning() << "Query failed";
        qWarning() << query.lastError();
        qWarning() << statement;
        return false;
    } else {
        return true;
    }
}

static bool prepareDatabase(QSqlDatabase &database)
{
    if (!database.transaction())
        return false;

    bool error = false;
    for (int i = 0; i < db_schema_count; ++i) {
        QSqlQuery query(database);

        if (!query.exec(QLatin1String(db_schema[i]))) {
            qWarning() << "Table creation failed";
            qWarning() << query.lastError();
            qWarning() << db_schema[i];
            error = true;
            break;
        }
    }

    if (error) {
        database.rollback();
        return false;
    } else {
        return database.commit();
    }
}

QSqlDatabase CommHistoryDatabase::open(const QString &databaseName)
{
    // horrible hack: Qt4 didn't have GenericDataLocation so we hardcode database location.
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QDir databaseDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String(COMMHISTORY_DATABASE_DIR));
#else
    QDir databaseDir(QDesktopServices::storageLocation(QDesktopServices::HomeLocation) + QLatin1String("/.local/share") + QLatin1String(COMMHISTORY_DATABASE_DIR));
#endif

    if (!databaseDir.exists())
        databaseDir.mkpath(QLatin1String("."));

    const QString databaseFile = databaseDir.absoluteFilePath(QLatin1String(COMMHISTORY_DATABASE_NAME));
    const bool exists = QFile::exists(databaseFile);

    QSqlDatabase database = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), databaseName);
    database.setDatabaseName(databaseFile);

    if (!database.open()) {
        qWarning() << "Failed to open commhistory database";
        qWarning() << database.lastError();
        return database;
    } else {
        qWarning() << "Opened commhistory database:" << databaseFile;
    }

    for (int i = 0; i < db_setup_count; i++) {
        if (!execute(database, QLatin1String(db_setup[i]))) {
            database.close();
            if (!exists)
                QFile::remove(databaseFile);
            return database;
        }
    }

    if (!exists && !prepareDatabase(database)) {
        database.close();
        QFile::remove(databaseFile);
    }

    return database;
}

QSqlQuery CommHistoryDatabase::prepare(const char *statement, const QSqlDatabase &database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    if (!query.prepare(statement)) {
        qWarning() << "Failed to prepare query";
        qWarning() << query.lastError();
        qWarning() << statement;
        return QSqlQuery();
    }
    return query;
}

