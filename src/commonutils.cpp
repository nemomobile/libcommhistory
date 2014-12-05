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

#include "commonutils.h"
#include "libcommhistoryexport.h"

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_impl.h>

#include <QString>
#include <QSettings>

namespace {

int phoneNumberMatchLength()
{
    // TODO: use a configuration variable to make this configurable
    static int numberMatchLength = QtContactsSqliteExtensions::DefaultMaximumPhoneNumberCharacters;
    return numberMatchLength;
}

}

namespace CommHistory {

LIBCOMMHISTORY_EXPORT QString normalizePhoneNumber(const QString &number, bool validate)
{
    // Validate the number, and retain the dial string
    QtContactsSqliteExtensions::NormalizePhoneNumberFlags flags(QtContactsSqliteExtensions::KeepPhoneNumberDialString);
    if (validate)
        flags |= QtContactsSqliteExtensions::ValidatePhoneNumber;

    return QtContactsSqliteExtensions::normalizePhoneNumber(number, flags);
}

LIBCOMMHISTORY_EXPORT QString minimizePhoneNumber(const QString &number)
{
    return QtContactsSqliteExtensions::minimizePhoneNumber(number, phoneNumberMatchLength());
}

LIBCOMMHISTORY_EXPORT bool remoteAddressMatch(const QString &localUid, const QString &uid, const QString &match, bool minimizedComparison)
{
    if (localUidComparesPhoneNumbers(localUid)) {
        QString phone, phoneMatch;
        if (minimizedComparison) {
            phone = minimizePhoneNumber(uid);
            phoneMatch = minimizePhoneNumber(match);
        } else {
            phone = normalizePhoneNumber(uid, false);
            phoneMatch = normalizePhoneNumber(match, false);
        }

        // If normalization returned an empty number (no digits), compare the input instead
        if (phone.isEmpty())
            phone = uid;
        if (phoneMatch.isEmpty())
            phoneMatch = match;

        return phoneMatch.compare(phone, Qt::CaseInsensitive) == 0;
    } else {
        return match.compare(uid, Qt::CaseInsensitive) == 0;
    }
}

LIBCOMMHISTORY_EXPORT bool remoteAddressMatch(const QString &localUid, const QStringList &originalUids, const QStringList &originalMatches, bool minimizedComparison)
{
    if (originalUids.size() != originalMatches.size())
        return false;

    QStringList uids;
    foreach (QString uid, originalUids) {
        if (localUidComparesPhoneNumbers(localUid)) {
            QString number = minimizedComparison ? minimizePhoneNumber(uid) : normalizePhoneNumber(uid, false);
            if (!number.isEmpty())
                uid = number;
        }

        uids.append(uid);
    }

    QStringList matches;
    foreach (QString match, originalMatches) {
        if (localUidComparesPhoneNumbers(localUid)) {
            QString number = minimizedComparison ? minimizePhoneNumber(match) : normalizePhoneNumber(match, false);
            if (!number.isEmpty())
                match = number;
        }

        matches.append(match);
    }

    uids.sort(Qt::CaseInsensitive);
    matches.sort(Qt::CaseInsensitive);

    for (int i = 0; i < uids.size(); i++) {
        if (uids[i].compare(matches[i], Qt::CaseInsensitive) != 0)
            return false;
    }

    return true;
}

}
