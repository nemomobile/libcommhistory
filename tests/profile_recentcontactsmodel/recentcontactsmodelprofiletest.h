/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#ifndef RECENTCONTACTSMODELPROFILETEST_H
#define RECENTCONTACTSMODELPROFILETEST_H

#include <QObject>
#include <QFile>
#include <QStringList>

class RecentContactsModelProfileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void prepare();
    void execute();
    void finalise();
    void cleanupTestCase();

private:
    QFile *logFile;
    QStringList remoteUids;
    QList<int> contactIndices;
};

#endif
