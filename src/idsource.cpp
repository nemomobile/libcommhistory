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

#include <QDir>
#include <QDebug>

#include "idsource.h"

#define LAST_IDS_DIR "/.commhistoryd/"
#define LAST_IDS_FILE LAST_IDS_DIR "ids.dat"

using namespace CommHistory;

static const int BASKET_SIZE = 5; //bits
static const int BASKET_FULL = 0x1F;

namespace CommHistory {
struct IdSourceData {
    int lastEventId;
    int lastGroupId;
    int users;
};
}

IdSource::IdSource(QObject *parent) :
    QObject(parent),
    m_File(QDir::homePath()
           + QLatin1String(LAST_IDS_FILE))
{
}

IdSource::~IdSource()
{
    if (m_IdSource.isAttached()) {
        IdSourceData *ids = lockSharedData();
        ids->users--;
        save(ids);
        m_IdSource.unlock();

        m_IdSource.detach();
    }
}

IdSourceData* IdSource::lockSharedData()
{
    Q_ASSERT(m_IdSource.isAttached());
    m_IdSource.lock();
    return reinterpret_cast<IdSourceData*>(m_IdSource.data());
}

int IdSource::skipToNextBasket(int currentId)
{
    return ((currentId >> BASKET_SIZE) + 1)<< BASKET_SIZE;
}

bool IdSource::newBasket(int currentId) const
{
    return !(currentId & BASKET_FULL);
}

int IdSource::nextEventId()
{
    int nextId = 0;

    if (openSharedMemory()) {
        IdSourceData *ids = lockSharedData();
        nextId = ++ids->lastEventId;
        if (newBasket(ids->lastEventId))
            save(ids);
        m_IdSource.unlock();
    }

    return nextId;
}

int IdSource::nextGroupId()
{
    int nextId = 0;

    if (openSharedMemory()) {
        IdSourceData *ids = lockSharedData();
        nextId = ++ids->lastGroupId;
        if (newBasket(ids->lastGroupId))
            save(ids);
        m_IdSource.unlock();
    }

    return nextId;
}

bool IdSource::openSharedMemory()
{
    if (!m_IdSource.isAttached()) {
        m_IdSource.setKey(QLatin1String("CommHistoryIdSource"));

        if (m_IdSource.attach()) {
            IdSourceData *ids = lockSharedData();
            ids->users++;
            m_IdSource.unlock();
        } else if (m_IdSource.error() == QSharedMemory::NotFound) {

            QDir dir(QDir::homePath() + QLatin1String(LAST_IDS_DIR));
            if (!dir.exists()) {
                if (!dir.mkpath(dir.path()))
                    qWarning() << "Failed create dir" << dir.path();
                return false;
            }

            if (m_IdSource.create(sizeof(IdSource))) {
                IdSourceData *ids = lockSharedData();

                ids->lastEventId = 0;
                ids->lastGroupId = 0;
                ids->users = 0;

                load(ids);

                if (ids->users != 0) {
                    qWarning() << "Skip ids after crash";
                    ids->lastEventId = skipToNextBasket(ids->lastEventId);
                    ids->lastGroupId = skipToNextBasket(ids->lastGroupId);
                    ids->users = 1;
                } else {
                    ids->users++;
                }
                save(ids);

                m_IdSource.unlock();
            } else {
                qCritical() << Q_FUNC_INFO
                        << "Failed create shared mem"
                        << m_IdSource.errorString();
                return false;
            }
        } else {
            qCritical() << m_IdSource.errorString();
            return false;
        }
    }

    return true;
}

void IdSource::load(IdSourceData *data)
{
    if (m_File.open(QIODevice::ReadOnly)) {
        int read = m_File.read(reinterpret_cast<char*>(data),
                               sizeof(IdSourceData));
        if (read != sizeof(IdSourceData))
            qWarning() << "Failed read from "<< m_File.fileName();
        m_File.close();
    } // it fails for the very first time when the file does not exist
}

void IdSource::save(IdSourceData *data)
{
    if (m_File.open(QIODevice::WriteOnly)) {
        int written = m_File.write(reinterpret_cast<char*>(data),
                                   sizeof(IdSourceData));
        if (written!= sizeof(IdSourceData))
            qWarning() << "Failed to write to "<< m_File.fileName();
        m_File.close();
    } else {
        qWarning() << "Failed open " << m_File.fileName();
    }
}
