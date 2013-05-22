/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#ifndef __QCONTACTTPMETADATA_P_H__
#define __QCONTACTTPMETADATA_P_H__

#include <QContactDetail>
#include <QContactDetailFilter>

class Q_CONTACTS_EXPORT QContactTpMetadata : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(QContactTpMetadata, "TpMetadata")
    Q_DECLARE_LATIN1_CONSTANT(FieldContactId, "ContactId");
    Q_DECLARE_LATIN1_CONSTANT(FieldAccountId, "AccountId");
    Q_DECLARE_LATIN1_CONSTANT(FieldAccountEnabled, "AccountEnabled");

    void setContactId(const QString &s);
    QString contactId() const;

    void setAccountId(const QString &s);
    QString accountId() const;

    void setAccountEnabled(bool b);
    bool accountEnabled() const;

    static QContactDetailFilter matchContactId(const QString &s);

    static QContactDetailFilter matchAccountId(const QString &s);
};

#endif
