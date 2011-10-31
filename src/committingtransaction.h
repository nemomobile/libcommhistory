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

#ifndef COMMHISTORY_COMMITTINGTRANSACTION_H
#define COMMHISTORY_COMMITTINGTRANSACTION_H

#include <QObject>
#include <QVariant>

class QSparqlConnection;
class QSparqlQuery;

#include "libcommhistoryexport.h"

namespace CommHistory {

class CommittingTransactionPrivate;

class LIBCOMMHISTORY_EXPORT CommittingTransaction : public QObject
{
    Q_OBJECT

public:
    CommittingTransaction(QObject *parent = 0);
    ~CommittingTransaction();

    /*!
     * Add delayed signal to be send when transaction finished.
     *
     * \param onError indicates to emit the signal on failed transaction, otherwise it's
     *                emitted on successful transaction
     * \param singalName
     * \param sender signale sender
     * \param arg1 signal 1st argument
     * \param arg2 signal 2nd argument
     */
    void addSignal(bool onError,
                   QObject *sender,
                   const char *signalName,
                   QGenericArgument arg1,
                   QGenericArgument arg2 = QGenericArgument());

    bool run(QSparqlConnection &connection, bool isBlocking = false);

    bool isRunning() const;
    bool isFinished() const;

    void abort(bool isError = true);

    /*!
     * Add query to be executed within the transaction, with an optional callback.
     * The callback slot will be called after the query has finished and must
     * have the signature (CommittingTransaction *t, QSparqlResult *result,
     * QVariant arg). The result is deleted by the transaction.
     */
    void addQuery(const QSparqlQuery &query,
                  QObject *caller = 0,
                  const char *callback = 0,
                  QVariant arg = QVariant());

Q_SIGNALS:
    void finished();

private:
    friend class TrackerIO;
    friend class TrackerIOPrivate;
    friend class CommittingTransactionPrivate;
    CommittingTransactionPrivate * const d;
};

} // namespace

#endif
