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

#include <QDebug>

#include <QSparqlResult>
#include <QSparqlError>

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

    foreach (QPointer<QSparqlResult> r, results) {
        if (r)
            delete r.data();
    }
}

void CommittingTransactionPrivate::addResult(QSparqlResult *result)
{
    if (result) {
        results.append(QPointer<QSparqlResult>(result));
        connect(result, SIGNAL(finished()),
                SLOT(finished()));
        started = true;
    }
}

void CommittingTransactionPrivate::addQuery(const  QSparqlQuery &query)
{
    pendingQueries.append(query);
}

QList<QSparqlQuery> CommittingTransactionPrivate::queries() const
{
    return pendingQueries;
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

    QMutableListIterator<QPointer<QSparqlResult> > i(results);
    while (i.hasNext()) {
        i.next();
        if (i.value().data() == currentResult) {
            i.remove();
            currentResult->deleteLater();
            break;
        }
    }

    if (results.isEmpty()) {
        foreach (DelayedSignal s, modelSignals) {
            if (error == s.onError
                && s.sender) {
                if (s.arguments.size() == 1) {
                    QMetaObject::invokeMethod(s.sender.data(),
                                              s.signalName,
                                              QGenericArgument(s.arguments[0].typeName,
                                                               s.arguments[0].data));
                } else if (s.arguments.size() == 2) {
                    QMetaObject::invokeMethod(s.sender.data(),
                                              s.signalName,
                                              QGenericArgument(s.arguments[0].typeName,
                                                               s.arguments[0].data),
                                              QGenericArgument(s.arguments[1].typeName,
                                                               s.arguments[1].data));
                }
            }
        }

        emit q->finished();
    }
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

bool CommittingTransaction::isRunning() const
{
    return !d->results.isEmpty();
}

bool CommittingTransaction::isFinished() const
{
    return d->started && d->results.isEmpty();
}

} // namespace CommHistory
