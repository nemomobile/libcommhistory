/* * This file is part of libcommhistory *
 *
 * Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Alvi Hirvela <alvi.hirvela@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all
 * of this material requires the prior written consent of Nokia
 * Corporation. This material also contains confidential information
 * which may not be disclosed to others without the prior written
 * consent of Nokia.
*/

#ifndef COMMHISTORY_COMMITTINGTRANSACTION_P_H
#define COMMHISTORY_COMMITTINGTRANSACTION_P_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QWeakPointer>
#include <QMetaType>

#include <QSparqlQuery>

class QSparqlResult;

namespace CommHistory {

class CommittingTransaction;

class CommittingTransactionPrivate : public QObject
{
    Q_OBJECT

public:
    CommittingTransactionPrivate(CommittingTransaction *parent) :
            q(parent),
            error(false),
            started(false)
    {
    }

    ~CommittingTransactionPrivate();

    CommittingTransaction *q;

    void addQuery(const QSparqlQuery &query);
    QList<QSparqlQuery> queries() const;

    void addResult(QSparqlResult *result);

private Q_SLOTS:
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

    QList<QSparqlQuery> pendingQueries;
    QList<QPointer<QSparqlResult> > results;
    bool error;
    bool started;

    friend class CommittingTransaction;
};

}

#endif
