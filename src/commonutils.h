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

#ifndef COMMHISTORY_COMMONUTILS_H
#define COMMHISTORY_COMMONUTILS_H

#include <QString>

// FIXME: keep this for now to avoid API break
#define PHONE_NUMBER_MATCH_LENGTH 7

namespace CommHistory {

enum PhoneNumberNormalizeFlag
{
    NormalizeFlagNone = 0,

    /* Remove +() and spaces */
    NormalizeFlagRemovePunctuation = 1,

    /* Don't remove [#xwp].* */
    NormalizeFlagKeepDialString = 2
};

Q_DECLARE_FLAGS(PhoneNumberNormalizeFlags, CommHistory::PhoneNumberNormalizeFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(PhoneNumberNormalizeFlags);

/*!
 * Validates and normalizes a phone number by removing extra characters:
 * "+358 012 (34) 567#3333" -> "+35801234567"
 *
 * \param number Phone number.
 * \param flags See CommHistory::PhoneNumberNormalizeFlags.
 * \return normalized number, or empty string if invalid.
 */
QString normalizePhoneNumber(const QString &number,
                             PhoneNumberNormalizeFlags flags = NormalizeFlagRemovePunctuation);

/*!
 * Compares the two remote ids. In case of phone numbers, last digits
 * are compared.
 *
 * \param uid First remote id.
 * \param match Second remote id.
 * \param flags With NormalizeFlagKeepDialString, keep the dial string
 * part (e.g. "p123"), if any, in the shortened number when comparing.
 * \return true if addresses match.
 */
bool remoteAddressMatch(const QString &uid,
                        const QString &match,
                        PhoneNumberNormalizeFlags flags = NormalizeFlagRemovePunctuation);

/*!
 * \return how many last digits are compared when matching phone
 * numbers, obtained from system-wide settings.
 */
int phoneNumberMatchLength();

/*!
 * Get the last digits (see phoneNumberMatchLength) of a phone number
 * for comparison purposes.
 *
 * \param number Phone number.
 * \param flags With NormalizeFlagKeepDialString, append the dial string
 * part (e.g. "p123") to the shortened number.
 * \return Last digits of the number.
 */

QString makeShortNumber(const QString &number,
                        PhoneNumberNormalizeFlags flags = NormalizeFlagRemovePunctuation);

}

#endif /* COMMONUTILS_H */
