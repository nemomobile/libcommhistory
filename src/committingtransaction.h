/* * This file is part of libcommhistory *
 *
 * Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef COMMHISTORY_COMMITTINGTRANSACTION_H
#define COMMHISTORY_COMMITTINGTRANSACTION_H

#include <QObject>

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

    bool isRunning() const;
    bool isFinished() const;

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
