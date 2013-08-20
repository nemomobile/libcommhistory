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
 * Validates and normalizes a phone number by removing extra characters:
 * "+358 012 (34) 567#3333" -> "+35801234567#3333"
 *
 * \param number Phone number.
 * \return normalized number, or empty string if invalid.
 */
QString normalizePhoneNumber(const QString &number);

/*!
 * Compares the two remote ids. In case of phone numbers, last digits
 * are compared.
 *
 * \param uid First remote id.
 * \param match Second remote id.
 * \return true if addresses match.
 */
bool remoteAddressMatch(const QString &uid, const QString &match);

/*!
 * Get the last digits (see phoneNumberMatchLength) of a phone number
 * for comparison purposes.
 *
 * \param number Phone number.
 * \return Last digits of the number.
 */

QString makeShortNumber(const QString &number);

}

#endif /* COMMONUTILS_H */
