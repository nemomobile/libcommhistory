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

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/tracker.h>
#include "eventmodel_p.h"
#include "syncsmsmodel.h"
#include "trackerio.h"

#include <QtSql>

namespace CommHistory
{

    class SyncSMSModelPrivate: public EventModelPrivate
    {
    public:
        SyncSMSModelPrivate(EventModel* model, int _parentId, QDateTime _dtTime, bool _lastModified)
            : EventModelPrivate(model)
            , dtTime(_dtTime)
            , parentId(_parentId)
            , lastModified(_lastModified)
        {
            Event::PropertySet properties;
            properties += Event::Id;
            properties += Event::Type;
            properties += Event::StartTime;
            properties += Event::EndTime;
            properties += Event::Direction;
            properties += Event::IsDraft;
            properties += Event::IsRead;
            properties += Event::LocalUid;
            properties += Event::RemoteUid;
            properties += Event::ParentId;
            properties += Event::FreeText;
            properties += Event::GroupId;
            properties += Event::LastModified;
            properties += Event::Encoding;
            properties += Event::CharacterSet;
            properties += Event::Language;
            propertyMask = properties;
        }

        bool acceptsEvent(const Event &event) const {
            qDebug() << __PRETTY_FUNCTION__ << event.id();

            if (event.type() != Event::SMSEvent)
                return false;

            if (!lastModified) {
                if (!dtTime.isNull()) {
                    return (event.endTime() > dtTime);
                } else {
                    return true;
                }
            }

            if (!dtTime.isNull() && ((event.endTime() > dtTime)
                || (event.lastModified() <= dtTime))) {
                return false;
            } else if (lastModified && (event.lastModified() <= dtTime)) {
                return false;
            }

            return true;
        }

        QDateTime dtTime;
        int parentId;
        bool lastModified;
    };

}

using namespace CommHistory;

SyncSMSModel::SyncSMSModel(int parentId , QDateTime time, bool lastModified ,QObject *parent)
    : EventModel(*(new SyncSMSModelPrivate(this, parentId, time, lastModified)), parent)
{

}

SyncSMSModel::~SyncSMSModel()
{
}

bool SyncSMSModel::getEvents()
{
    Q_D(SyncSMSModel);

    reset();
    d->clearEvents();

    RDFVariable message = RDFVariable::fromType<nmo::SMSMessage>();
    if (d->parentId != ALL) {
        message.property<nmo::phoneMessageId>(LiteralValue(d->parentId));
    }

    if (d->lastModified) {
        if (!d->dtTime.isNull()) { //get all last modified messages after time t1
            message.property<tracker::added>().lessOrEqual(LiteralValue(d->dtTime));
            message.property<nie::contentLastModified>().greater(LiteralValue(d->dtTime));
        } else {
            message.property<nmo::contentLastModified>().greater(LiteralValue(QDateTime::fromTime_t(0)));
        }
    } else {
        if (!d->dtTime.isNull()) { //get all messages after time t1(including modified)
            message.property<tracker::added>().greater(LiteralValue(d->dtTime));
        }
    }

    RDFSelect query;
    d->tracker()->prepareMessageQuery(query, message, d->propertyMask);
    return d->executeQuery(query);
}

void SyncSMSModel::setSyncSmsFilter(const SyncSMSFilter& filter)
{
    Q_D(SyncSMSModel);
    d->parentId = filter.parentId;
    d->dtTime = filter.time;
    d->lastModified = filter.lastModified;
}

QList<FolderInfo> SyncSMSModel::folderInfo() const
{
    return QList<FolderInfo>();
}

QSqlError SyncSMSModel::addPrivateFolders(QList<FolderInfo> listFolderInfo)
{
    Live<nmo::SMSFolder> myFolder = ::tracker()->strictLiveNode(nmo::iri("predefined-phone-msg-folder-myfolder"));
    QSqlError errStatus;
    if (!myFolder) {
        errStatus.setType(QSqlError::TransactionError);
        errStatus.setDatabaseText(QLatin1String("Cannot find default MyFolder"));
        return errStatus;
    }

    foreach (FolderInfo info, listFolderInfo) {
        QString folderId = QString(QLatin1String("0x%1")).arg(info.folderId, 0, 16);
        QString url_folderId = QString(QLatin1String("sms-folder-myfolder-%1")).arg(folderId);;
        QUrl folderUrl(url_folderId);
        Live<nmo::SMSFolder> folder = ::tracker()->liveNode(folderUrl);
        folder->setPhoneMessageFolderId(folderId);
        folder->setTitle(info.folderName);
        folder->setContentCreated(QDateTime::currentDateTime()); //setting created time
        myFolder->addContainsPhoneMessageFolder(folder);
    }

    if (!listFolderInfo.isEmpty()) { //if there have been any folders added
        myFolder->setContentLastModified(QDateTime::currentDateTime()); //MyFolder modified time
    }
    return errStatus;
}

bool SyncSMSModel::folderExists(int id)
{
    QString folderId = QString(QLatin1String("0x%1")).arg(id, 0, 16);
    QString folderUrl = QString(QLatin1String("sms-folder-myfolder-%1")).arg(folderId);;
    Live<nmo::SMSFolder> myFolder = ::tracker()->strictLiveNode(QUrl(folderUrl));
    if (!myFolder) {
        qDebug() << "Cannot find My Folder with id" <<  folderId;
        return false;
    }
    return true;
}
