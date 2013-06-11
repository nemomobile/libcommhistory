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

#include "qcontacttpmetadata_p.h"

void QContactTpMetadata::setContactId(const QString &s)
{
    setValue(FieldContactId, s);
}

QString QContactTpMetadata::contactId() const
{
    return value<QString>(FieldContactId);
}

void QContactTpMetadata::setAccountId(const QString &s)
{
    setValue(FieldAccountId, s);
}

QString QContactTpMetadata::accountId() const
{
    return value<QString>(FieldAccountId);
}

void QContactTpMetadata::setAccountEnabled(bool b)
{
    setValue(FieldAccountEnabled, QLatin1String(b ? "true" : "false"));
}

bool QContactTpMetadata::accountEnabled() const
{
    return value<bool>(FieldAccountEnabled);
}

QContactDetailFilter QContactTpMetadata::matchContactId(const QString &s)
{
    QContactDetailFilter filter;
#ifdef USING_QTPIM
    filter.setDetailType(QContactTpMetadata::Type, FieldContactId);
#else
    filter.setDetailDefinitionName(QContactTpMetadata::DefinitionName, FieldContactId);
#endif
    filter.setValue(s);
    filter.setMatchFlags(QContactFilter::MatchExactly);
    return filter;
}

QContactDetailFilter QContactTpMetadata::matchAccountId(const QString &s)
{
    QContactDetailFilter filter;
#ifdef USING_QTPIM
    filter.setDetailType(QContactTpMetadata::Type, FieldAccountId);
#else
    filter.setDetailDefinitionName(QContactTpMetadata::DefinitionName, FieldAccountId);
#endif
    filter.setValue(s);
    filter.setMatchFlags(QContactFilter::MatchExactly);
    return filter;
}

#ifdef USING_QTPIM
const QContactDetail::DetailType QContactTpMetadata::Type(static_cast<QContactDetail::DetailType>(QContactDetail::TypeVersion + 1));
#else
Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactTpMetadata, "TpMetadata");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldContactId, "ContactId");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldAccountId, "AccountId");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldAccountEnabled, "AccountEnabled");
#endif

