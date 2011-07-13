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

#include <QDebug>

#include <QSparqlResult>
#include <QSparqlError>
#include <QTimer>

#include "committingtransaction.h"
#include "committingtransaction_p.h"

namespace CommHistory
{

void CommittingTransactionPrivate::DelayedSignal::addArgument(QGenericArgument &arg)
{
    if (qstrlen(arg.name())) {
        int type = QMetaType::type(arg.name());
        if (type) {
            SignalArgument sa;
            sa.typeName = arg.name();
            sa.data = QMetaType::construct(type, arg.data());

            arguments.append(sa);
        } else {
            qCritical() << Q_FUNC_INFO << "Invalid type" << arg.name();
        }
    }
}

CommittingTransactionPrivate::~CommittingTransactionPrivate()
{
    foreach (DelayedSignal s, modelSignals) {
        foreach (DelayedSignal::SignalArgument sa, s.arguments) {
            int type = QMetaType::type(sa.typeName);
            if (type)
                QMetaType::destroy(type, sa.data);
            else
                qCritical() << "Invalid type" << sa.typeName;
        }
    }

    if (!pendingQueries.isEmpty()) {
        QPointer<QSparqlResult> result = pendingQueries.first()->result;
        if (result)
            result->deleteLater();
    }

    qDeleteAll(pendingQueries);
    pendingQueries.clear();
}

bool CommittingTransactionPrivate::runNextQuery()
{
    qDebug() << Q_FUNC_INFO;

    PendingQuery *query = pendingQueries.first();
    query->result = connection->exec(query->query);
    if (query->result->hasError()) {
        qWarning() << query->result->lastError().message();
        return false;
    }

    connect(query->result, SIGNAL(finished()), SLOT(finished()));
    // delete result out of the slot, workaround qsparl bugs for insert queries
    connect(query->result, SIGNAL(finished()), query->result, SLOT(deleteLater()), Qt::QueuedConnection);
    started = true;

    return true;
}

void CommittingTransactionPrivate::addQuery(const QSparqlQuery &query,
                                            QObject *caller,
                                            const char *callback,
                                            QVariant argument)
{
    CommittingTransactionPrivate::PendingQuery *p = new CommittingTransactionPrivate::PendingQuery;
    p->query = query;
    p->result = 0;
    p->caller = caller;
    p->callback = callback;
    p->argument = argument;

    pendingQueries.append(p);
}

bool CommittingTransactionPrivate::isEmpty() const
{
    return pendingQueries.isEmpty();
}

void CommittingTransactionPrivate::handleCallbacks(PendingQuery *query)
{
    qDebug() << Q_FUNC_INFO;

    if (query->callback && query->caller.data()) {
        QMetaObject::invokeMethod(query->caller.data(), query->callback,
                                  Q_ARG(CommittingTransaction *, q),
                                  Q_ARG(QSparqlResult *, query->result),
                                  Q_ARG(QVariant, query->argument));
    }
}

void CommittingTransactionPrivate::sendSignals()
{
    qDebug() << Q_FUNC_INFO;

    foreach (DelayedSignal s, modelSignals) {
        if (error == s.onError
            && s.sender) {
            if (s.arguments.size() == 1) {
                qDebug() << s.signalName;
                QMetaObject::invokeMethod(s.sender.data(),
                                          s.signalName,
                                          QGenericArgument(s.arguments[0].typeName,
                                                           s.arguments[0].data));
            } else if (s.arguments.size() == 2) {
                qDebug() << s.signalName;
                QMetaObject::invokeMethod(s.sender.data(),
                                          s.signalName,
                                          QGenericArgument(s.arguments[0].typeName,
                                                           s.arguments[0].data),
                                          QGenericArgument(s.arguments[1].typeName,
                                                           s.arguments[1].data));
            }
        }
    }
}

void CommittingTransactionPrivate::finished()
{
    qDebug() << Q_FUNC_INFO;

    QSparqlResult *currentResult = qobject_cast<QSparqlResult*>(sender());
    if (currentResult) {
        bool isError = currentResult->hasError();
        if (isError) {
            error = isError;
            qWarning() << Q_FUNC_INFO << currentResult->lastError().message();
        }
    }

    PendingQuery *query = pendingQueries.takeFirst();
    handleCallbacks(query);
    delete query;

    if (!pendingQueries.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(runNextQuery()));
        return;
    }

    // all done
    sendSignals();
    emit q->finished();
}

CommittingTransaction::CommittingTransaction(QObject *parent) : QObject(parent),
        d(new CommittingTransactionPrivate(this))
{
}

CommittingTransaction::~CommittingTransaction()
{
    delete d;
}

void CommittingTransaction::addSignal(bool onError,
                                      QObject *sender,
                                      const char *signalName,
                                      QGenericArgument arg1,
                                      QGenericArgument arg2)
{
    int type = QMetaType::type(arg1.name());
    if (type) {
        CommittingTransactionPrivate::DelayedSignal s;
        s.onError = onError;
        s.sender = sender;
        s.signalName = signalName;
        s.addArgument(arg1);
        s.addArgument(arg2);

        d->modelSignals.append(s);
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid type " << arg1.name();
    }
}

bool CommittingTransaction::run(QSparqlConnection &connection, bool isBlocking)
{
    qDebug() << Q_FUNC_INFO;

    if (isRunning()) return true;

    d->connection = &connection;

    if (!isBlocking)
        return d->runNextQuery();

    d->started = true;
    while (d->pendingQueries.isEmpty()) {
        CommittingTransactionPrivate::PendingQuery *query = d->pendingQueries.first();
        query->result = connection.exec(query->query);
        if (query->result->hasError()) {
            qWarning() << query->result->lastError().message();
            return false;
        }

        query->result->waitForFinished();

        if (query->result->hasError()) {
            qWarning() << query->result->lastError().message();
        } else {
            d->handleCallbacks(query);
        }

        d->pendingQueries.removeFirst();
        delete query;
    }

    d->sendSignals();

    return true;
}

bool CommittingTransaction::isRunning() const
{
    return d->started;
}

bool CommittingTransaction::isFinished() const
{
    return d->started && d->pendingQueries.isEmpty();
}

} // namespace CommHistory
