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

namespace CommHistory {

/*!
 * Whether the given localUid needs phone number comparsions
 *
 * \param localUid Local UID
 * \return true if remote UIDs will be compared as phone numbers
 */
inline bool localUidComparesPhoneNumbers(const QString &localUid)
{
    return localUid.startsWith(QLatin1String("/org/freedesktop/Telepathy/Account/ring/"));
}

/*!
 * Validates and normalizes a phone number by removing extra characters:
 * "+358 012 (34) 567#3333" -> "+35801234567#3333"
 *
 * Do not use this function to determine whether an input is a phone
 * number; that decision should be made based on localUid.
 *
 * \param number Phone number.
 * \param validate Validate phone number (returns empty if invalid)
 * \return normalized number, or empty string if validating and invalid.
 */
QString normalizePhoneNumber(const QString &number, bool validate);

/*!
 * Get the last digits of a phone number for comparison purposes.
 *
 * \param number Phone number.
 * \return Last digits of the number.
 */
QString minimizePhoneNumber(const QString &number);

/*!
 * Compares the two remote ids. In case of phone numbers, last digits
 * are compared.
 *
 * \param localUid Local UID to determine comparison type
 * \param uid First remote id.
 * \param match Second remote id.
 * \param minimizedComparison Compare numbers in minimized form.
 * \return true if addresses match.
 */
bool remoteAddressMatch(const QString &localUid, const QString &uid, const QString &match, bool minimizedComparison = false);
bool remoteAddressMatch(const QString &localUid, const QStringList &uids, const QStringList &match, bool minimizedComparison = false);

}

#endif /* COMMONUTILS_H */
