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

#include "mmscontentdeleter.h"

#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QDateTime>

void MmsContentDeleter::deleteMessage(const QString &messageToken)
{
    if (!messageToken.isEmpty())
    {
        qDebug() << "[MMS-DELETER] Schedule message" << messageToken;
        QMetaObject::invokeMethod(this,
                                  "doMessageDelete",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, messageToken));
    }
}

void MmsContentDeleter::cleanMmsPlace()
{
    qDebug() << "[MMS-DELETER] Schedule cleaning mms place";

    QFileInfoList topEntryes;
    QDir public_dir(QString("%1/.mms/msg/").arg(QDir::homePath()));
    topEntryes << public_dir.entryInfoList(QStringList(),
                                       QDir::Dirs | QDir::Files
                                       | QDir::NoDotAndDotDot | QDir::Hidden
                                       | QDir::System);

    QDir private_dir(QString("%1/.mms/private/msg/").arg(QDir::homePath()));
    if (private_dir.exists())
    {
        topEntryes << private_dir.entryInfoList(QStringList(),
                                            QDir::Dirs | QDir::Files
                                            | QDir::NoDotAndDotDot | QDir::Hidden
                                            | QDir::System);
    }

    QDateTime garbageTime = QDateTime::currentDateTime().addDays(-1);
    qDebug() << "[MMS-DELETER] Garbage time is" << garbageTime;

    foreach (QFileInfo fi, topEntryes)
    {
        qDebug() << "[MMS-DELETER]" << fi.absoluteFilePath() << "time is" << fi.created();
        if(fi.created() <= garbageTime)
        {
            QMetaObject::invokeMethod(this,"doDeleteContent",
                                       Qt::QueuedConnection,
                                       Q_ARG(QString, fi.absoluteFilePath()));
        }
    }
}

QString MmsContentDeleter::resolveMessagePath(const QString &messageToken)
{
    QString retval;

    QDir public_dir(QString("%1/.mms/msg/%2").arg(QDir::homePath()).arg(messageToken));

    if (public_dir.exists()) {
        retval = public_dir.path();
    }

    QDir private_dir(QString("%1/.mms/private/msg/%2").arg(QDir::homePath()).arg(messageToken));

    if (private_dir.exists()) {
        retval = private_dir.path();
    }

    return retval;
}

void MmsContentDeleter::doMessageDelete(const QString &messageToken)
{
    QString messagePath = resolveMessagePath(messageToken);

    if (messagePath.isEmpty()) {
        qWarning() << "[MMS-DELETER] Failed to get message folder. " << messageToken;
    } else {
        qDebug() << "[MMS-DELETER] Delete message folder " << messagePath   << " Thread: " << thread();

        doDeleteContent(messagePath);
    }
}

void MmsContentDeleter::doDeleteContent(const QString &path)
{
    qDebug() << "[MMS-DELETER] Delete content. Path" << path;
    QFileInfo entry(path);
    if (!entry.exists())
    {
        qWarning() << "[MMS-DELETER] Path" << path << " does not exists.";
        return;
    }

    QDir parent = entry.dir();
    if (entry.isFile())
    {
        if (!parent.remove(entry.absoluteFilePath())) {
            qCritical() << "[MMS-DELETER] Can't delete file" << entry.absoluteFilePath();
        }
    }
    else if (entry.isDir())
    {
        QFile file(path);
        if (!file.setPermissions( QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther |
                                  QFile::ExeOwner  | QFile::ExeGroup  | QFile::ExeOther |
                                  QFile::WriteOwner ) )
        {
            qWarning() << "[MMS-DELETER] failed to chmod dir " << path << " error:" << file.errorString();
        }
        QDir dir(path);
        foreach (QFileInfo fi, dir.entryInfoList(QStringList(),
                                                 QDir::Dirs | QDir::Files
                                                 | QDir::NoDotAndDotDot | QDir::Hidden
                                                 | QDir::System))
        {
            doDeleteContent(fi.absoluteFilePath());
        }
        if (!parent.rmdir(entry.absoluteFilePath())) {
            qCritical() << "[MMS-DELETER] Can't delete dir" << entry.absoluteFilePath();
        }
    }
    else if (entry.isSymLink())
    {
        if (!parent.remove(entry.absoluteFilePath())) {
            qCritical() << "[MMS-DELETER] Can't delete symlink" << entry.absoluteFilePath();
        }
    }
    else
    {
        qCritical() << "Unknow fs entry" << entry.absoluteFilePath() << entry.fileName();
    }
}

