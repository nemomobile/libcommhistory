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

const int DEFAULT_PHONE_NUMBER_MATCH_LENGTH = 7;
int numberMatchLength = 0;

int phoneNumberMatchLength()
{
    if (!numberMatchLength) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QLatin1String("Nokia"), QLatin1String("Contacts"));
        bool valid;
        int length = settings.value(QLatin1String("numberMatchLength")).toInt(&valid);

        if (valid && length)
            numberMatchLength = length;
        else
            numberMatchLength = DEFAULT_PHONE_NUMBER_MATCH_LENGTH;
    }

    return numberMatchLength;
}

}

namespace CommHistory {

LIBCOMMHISTORY_EXPORT QString normalizePhoneNumber(const QString &number)
{
    // Validate the number, and retain the dial string
    QtContactsSqliteExtensions::NormalizePhoneNumberFlags flags(QtContactsSqliteExtensions::ValidatePhoneNumber |
                                                                QtContactsSqliteExtensions::KeepPhoneNumberDialString);

    return QtContactsSqliteExtensions::normalizePhoneNumber(number, flags);
}

LIBCOMMHISTORY_EXPORT bool remoteAddressMatch(const QString &uid, const QString &match)
{
    QString phone = normalizePhoneNumber(uid);

    // IM
    if (phone.isEmpty() && uid != match)
        return false;

    // phone number
    QString uidRight = makeShortNumber(uid);
    QString matchRight = makeShortNumber(match);

    return uidRight == matchRight;
}

LIBCOMMHISTORY_EXPORT QString makeShortNumber(const QString &number)
{
    return QtContactsSqliteExtensions::minimizePhoneNumber(number, phoneNumberMatchLength());
}

}
