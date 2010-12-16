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

#include <QList>
#include <QtTracker/Tracker>
#include <QDebug>

namespace CommHistory {

class Event;
class Group;

// committing transactions
// store model signals that should be emitted on transaction commit
struct LIBCOMMHISTORY_EXPORT DelayedSignal {
    const char *signalName;
    struct {
        const char *typeName;
        void *data;
    } arg;
};

struct LIBCOMMHISTORY_EXPORT CommittingTransaction {
    SopranoLive::RDFTransactionPtr transaction;
    QList<DelayedSignal> modelSignals;
    QList<Event> events;
    QList<Group> groups;

    void addSignal(const char *signalName,
                   QGenericArgument arg1) {
        int type = QMetaType::type(arg1.name());
        if (type) {
            DelayedSignal s;
            s.signalName = signalName;
            s.arg.typeName = arg1.name();
            s.arg.data = QMetaType::construct(type, arg1.data());
            modelSignals.append(s);
        } else {
            qCritical() << "Invalid type " << arg1.name();
        }
    }

    void sendSignals(QObject *object) {
        foreach (DelayedSignal s, modelSignals) {
            QMetaObject::invokeMethod(object,
                                      s.signalName,
                                      QGenericArgument(s.arg.typeName,
                                                       s.arg.data));
            int type = QMetaType::type(s.arg.typeName);
            if (type)
                QMetaType::destroy(type, s.arg.data);
            else
                qCritical() << "Invalid type" << s.arg.typeName;
        }
    }
};

}

#endif
