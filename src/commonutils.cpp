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

#include <QString>
#include <QRegExp>
#include <QSettings>

#include "commonutils.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

static const int DEFAULT_PHONE_NUMBER_MATCH_LENGTH = 7;
static int numberMatchLength = 0;

LIBCOMMHISTORY_EXPORT QString normalizePhoneNumber(const QString &number,
                                                   PhoneNumberNormalizeFlags flags)
{
    QString result(number);

    QRegExp sipRegExp("^sips?:(.*)@");
    if (sipRegExp.indexIn(number) != -1)
        result = sipRegExp.cap(1);

    // artistic reinterpretation of Fremantle code...

    if (flags & NormalizeFlagRemovePunctuation) {
        result.remove(QRegExp("[()\\-\\. ]"));
        // check for invalid characters
        if (result.indexOf(QRegExp("[^\\d#\\*\\+XxWwPp]")) != -1) {
            return QString();
        }
    } else {
        if (result.indexOf(QRegExp("[^()\\-\\. \\d#\\*\\+XxWwPp]")) != -1) {
            return QString();
        }
    }

    if (!(flags & NormalizeFlagKeepDialString))
        result.replace(QRegExp("[XxWwPp].*"), "");

    // can't have + with control codes
    if ((result.indexOf(QLatin1String("*31#")) != -1 ||
         result.indexOf(QLatin1String("#31#")) != -1) &&
         result.indexOf('+') != -1) {
        return QString();
    }

    return result;
}

LIBCOMMHISTORY_EXPORT bool remoteAddressMatch(const QString &uid,
                                              const QString &match,
                                              PhoneNumberNormalizeFlags flags)
{
    QString phone = normalizePhoneNumber(uid);

    // IM
    if (phone.isEmpty() && uid != match)
        return false;

    // phone number
    QString uidRight = makeShortNumber(uid, flags);
    QString matchRight = makeShortNumber(match, flags);

    return uidRight == matchRight;
}

LIBCOMMHISTORY_EXPORT int phoneNumberMatchLength()
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

LIBCOMMHISTORY_EXPORT QString makeShortNumber(const QString &number,
                                              PhoneNumberNormalizeFlags flags)
{
    QString normalized = normalizePhoneNumber(number);
    QString shortNumber = normalized.right(phoneNumberMatchLength());
    if (flags & NormalizeFlagKeepDialString) {
        QString normalizedWithString = normalizePhoneNumber(number,
                                                            NormalizeFlagRemovePunctuation
                                                            | NormalizeFlagKeepDialString);
        if (normalized != normalizedWithString) {
            QString dialString = normalizedWithString.mid(normalized.length());
            shortNumber.append(dialString);
        }
    }

    for (int i = 0; i < shortNumber.length(); i++) {
        int value = shortNumber.at(i).digitValue();
        if (value != -1) {
            shortNumber.replace(i, 1, QLatin1Char('0' + value));
        }
    }

    return shortNumber;
}

};
