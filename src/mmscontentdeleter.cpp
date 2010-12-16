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

#include "mmscontentdeleter.h"

#include <QDebug>
#include <QProcess>
#include <QDir>

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

        deleteDirWithContent(messagePath);
    }
}

void MmsContentDeleter::deleteDirWithContent(const QString &path)
{
    QDir dir(path);

    if (dir.exists()) {
        foreach (QFileInfo fi, dir.entryInfoList(QStringList(),
                                                 QDir::Dirs | QDir::Files
                                                 | QDir::NoDotAndDotDot | QDir::Hidden
                                                 | QDir::System)) {
            if (fi.isFile()) {
                dir.remove(fi.fileName());
            }
            else if (fi.isDir()) {
                deleteDirWithContent(fi.absoluteFilePath());
            }
            else if (fi.isSymLink()) {
                dir.remove(fi.fileName());
            }
            else {
                qWarning() << "Unknow fs entry" << fi.absoluteFilePath() << fi.fileName();
            }
        }

        QString dirName(dir.dirName());

        if (dir.cdUp()) {
            dir.rmdir(dirName);
        }
    }
}
