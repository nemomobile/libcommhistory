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
#include <QStandardPaths>

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
    "  parentId INTEGER, " // XXX remove, unused
    "  subject TEXT, "
    "  freeText TEXT, "
    "  groupId INTEGER, "
    "  messageToken TEXT, "
    "  lastModified INTEGER, "
    "  vCardFileName TEXT, "
    "  vCardLabel TEXT, "
    "  isDeleted INTEGER, " // XXX remove, unused
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
    "  hasExtraProperties BOOL DEFAULT 0, "
    "  FOREIGN KEY(groupId) REFERENCES Groups(id) ON DELETE CASCADE "
    ")",
    "CREATE INDEX events_remoteUid ON Events (remoteUid)",
    "CREATE INDEX events_type ON Events (type)",
    "CREATE INDEX events_messageToken ON Events (messageToken)",
    "CREATE INDEX events_sorting ON Events (groupId, endTime DESC, id DESC)",
    "CREATE INDEX events_unread ON Events (isRead)",

    "CREATE TABLE EventProperties ( "
    "  eventId INTEGER, "
    "  key TEXT, "
    "  value BLOB, "
    "  FOREIGN KEY (eventId) REFERENCES Events(id) ON DELETE CASCADE, "
    "  PRIMARY KEY (eventId, key) ON CONFLICT REPLACE "
    ")",

    "CREATE TRIGGER eventproperties_flag_insert AFTER INSERT ON EventProperties "
    "  BEGIN "
    "    UPDATE Events SET hasExtraProperties=1 WHERE id=NEW.eventId; "
    "  END",
    "CREATE TRIGGER eventproperties_flag_delete AFTER DELETE ON EventProperties "
    "  WHEN (SELECT COUNT(*) FROM EventProperties WHERE eventId=OLD.eventId) = 0 "
    "  BEGIN "
    "    UPDATE Events SET hasExtraProperties=0 WHERE id=OLD.eventId; "
    "  END",

    "PRAGMA user_version=3"
};
static int db_schema_count = sizeof(db_schema) / sizeof(*db_schema);

// Upgrade queries indexed by old version
static const char *db_upgrade_0[] = {
    "DROP INDEX events_groupId",
    "CREATE INDEX events_sorting ON Events (groupId, startTime DESC, id DESC)",
    "CREATE INDEX events_unread ON Events (isRead)",
    "PRAGMA user_version=1",
    0
};

static const char *db_upgrade_1[] = {
    "DROP INDEX events_sorting",
    "CREATE INDEX events_sorting ON Events (groupId, endTime DESC, id DESC)",
    "PRAGMA user_version=2",
    0
};

static const char *db_upgrade_2[] = {
    "ALTER TABLE Events ADD COLUMN hasExtraProperties BOOL DEFAULT 0",
    "CREATE TABLE EventProperties ( "
    "  eventId INTEGER, "
    "  key TEXT, "
    "  value BLOB, "
    "  FOREIGN KEY (eventId) REFERENCES Events(id) ON DELETE CASCADE, "
    "  PRIMARY KEY (eventId, key) ON CONFLICT REPLACE "
    ")",
    "CREATE TRIGGER eventproperties_flag_insert AFTER INSERT ON EventProperties "
    "  BEGIN "
    "    UPDATE Events SET hasExtraProperties=1 WHERE id=NEW.eventId; "
    "  END",
    "CREATE TRIGGER eventproperties_flag_delete AFTER DELETE ON EventProperties "
    "  WHEN (SELECT COUNT(*) FROM EventProperties WHERE eventId=OLD.eventId) = 0 "
    "  BEGIN "
    "    UPDATE Events SET hasExtraProperties=0 WHERE id=OLD.eventId; "
    "  END",
    "PRAGMA user_version=3",
    0
};

// REMEMBER TO UPDATE THE SCHEMA AND USER_VERSION!
static const char **db_upgrade[] = {
    db_upgrade_0,
    db_upgrade_1,
    db_upgrade_2
};
static int db_upgrade_count = sizeof(db_upgrade) / sizeof(*db_upgrade);

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

static bool upgradeDatabase(QSqlDatabase &database)
{
    QSqlQuery query(database);
    query.prepare("PRAGMA user_version");
    if (!query.exec() || !query.next()) {
        qWarning() << "User version query failed:" << query.lastError();
        return false;
    }

    int user_version = query.value(0).toInt();
    query.finish();

    while (user_version < db_upgrade_count) {
        qWarning() << "Upgrading commhistory database from schema version" << user_version;

        for (unsigned i = 0; db_upgrade[user_version][i]; i++) {
            if (!execute(database, QLatin1String(db_upgrade[user_version][i])))
                return false;
        }

        if (!query.exec() || !query.next()) {
            qWarning() << "User version query failed:" << query.lastError();
            return false;
        }

        user_version = query.value(0).toInt();
        query.finish();
    }

    if (user_version > db_upgrade_count)
        qWarning() << "Commhistory database schema is newer than expected - this may result in failures or corruption";

    return true;
}

QSqlDatabase CommHistoryDatabase::open(const QString &databaseName)
{
    QDir databaseDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String(COMMHISTORY_DATABASE_DIR));
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

    if (!exists) {
        if (!prepareDatabase(database)) {
            database.close();
            QFile::remove(databaseFile);
        }
    } else {
        if (!execute(database, "BEGIN EXCLUSIVE TRANSACTION")) {
            database.close();
            return database;
        }

        if (!upgradeDatabase(database) || !execute(database, "END TRANSACTION")) {
            execute(database, "ROLLBACK");
            qCritical() << "Database upgrade failed! Everything may break catastrophically.";
        }
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

