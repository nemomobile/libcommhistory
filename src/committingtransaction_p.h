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

#ifndef COMMHISTORY_COMMITTINGTRANSACTION_P_H
#define COMMHISTORY_COMMITTINGTRANSACTION_P_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QWeakPointer>
#include <QMetaType>

#include <QSparqlConnection>
#include <QSparqlQuery>

class QSparqlResult;

namespace CommHistory {

class CommittingTransaction;

class CommittingTransactionPrivate : public QObject
{
    Q_OBJECT

public:
    struct PendingQuery {
        QSparqlQuery query;
        QPointer<QSparqlResult> result;
        QWeakPointer<QObject> caller;
        const char *callback;
        QVariant argument;
    };

    CommittingTransactionPrivate(CommittingTransaction *parent) :
            q(parent),
            error(false),
            started(false)
    {
    }

    ~CommittingTransactionPrivate();

    CommittingTransaction *q;

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

    bool isEmpty() const;

    void handleCallbacks(PendingQuery *query);
    void sendSignals();

private Q_SLOTS:
    bool runNextQuery();
    void finished();

private:
    // committing transactions
    // store model signals that should be emitted on transaction commit
    struct DelayedSignal {
        const char *signalName;
        QWeakPointer<QObject> sender;
        bool onError;
        struct SignalArgument {
            const char *typeName;
            void *data;
        };
        QList<SignalArgument> arguments;

        void addArgument(QGenericArgument &arg);
    };

    QList<DelayedSignal> modelSignals;

    QSparqlConnection *connection;
    QList<PendingQuery *> pendingQueries;
    bool error;
    bool started;

    friend class CommittingTransaction;
};

}

#endif
