/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
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

#include "contentdeleter.h"

#include <QDir>
#include "event.h"
#include "debug.h"

void ContentDeleter::deleteMessage(int id)
{
    if (id) {
        DEBUG() << "[DELETER] Schedule message" << id;
        QMetaObject::invokeMethod(this, "doMessageDelete", Qt::QueuedConnection, Q_ARG(int, id));
    }
}

void ContentDeleter::doMessageDelete(int id)
{
    QDir dir = CommHistory::Event::dataDir(id, false);
    if (dir.exists()) {
        DEBUG() << "[DELETER] Delete message folder" << dir << "Thread:" << thread();
        doDeleteContent(dir.path());
    }
}

void ContentDeleter::doDeleteContent(QString path)
{
    DEBUG() << "[DELETER] Delete content. Path" << path;
    QFileInfo entry(path);
    if (!entry.exists()) {
        qWarning() << "[DELETER] Path" << path << " does not exists.";
        return;
    }

    QDir parent = entry.dir();
    if (entry.isFile()) {
        if (!parent.remove(entry.absoluteFilePath())) {
            qCritical() << "[DELETER] Can't delete file" << entry.absoluteFilePath();
        }
    } else if (entry.isDir()) {
        QFile file(path);
        if (!file.setPermissions(QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther |
                                 QFile::ExeOwner  | QFile::ExeGroup  | QFile::ExeOther |
                                 QFile::WriteOwner)) {
            qWarning() << "[DELETER] failed to chmod dir " << path << " error:" << file.errorString();
        }
        QDir dir(path);
        foreach (QFileInfo fi, dir.entryInfoList(QStringList(),
                                                 QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot |
                                                 QDir::Hidden | QDir::System)) {
            doDeleteContent(fi.absoluteFilePath());
        }
        if (!parent.rmdir(entry.absoluteFilePath())) {
            qCritical() << "[DELETER] Can't delete dir" << entry.absoluteFilePath();
        }
    } else if (entry.isSymLink()) {
        if (!parent.remove(entry.absoluteFilePath())) {
            qCritical() << "[DELETER] Can't delete symlink" << entry.absoluteFilePath();
        }
    } else {
        qCritical() << "Unknow fs entry" << entry.absoluteFilePath() << entry.fileName();
    }
}
