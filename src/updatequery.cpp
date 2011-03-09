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

#include <QStringBuilder>
#include <QSparqlBinding>

#include "updatequery.h"

#define LAT(s) QLatin1String(s)

namespace {

QString encloseUrl(const QUrl &url)
{
    QString urlString(url.toString());
    if (urlString.startsWith(LAT("_:"))
        || urlString.startsWith(LAT("?")))
        return urlString;

    return QString(LAT("<%1>")).arg(urlString);
}

void copyReverse(const QStringList &src, QStringList &target, const QString &delim = QString())
{
    QStringListIterator i(src);
    i.toBack();
    while(i.hasPrevious()) {
        if (!delim.isEmpty() && i.hasNext())
            target << delim;
        target << i.previous();
    }
}

}

namespace CommHistory {

UpdateQuery::UpdateQuery() : nextVar(0)
{
}


void UpdateQuery::deletion(const QUrl &subject,
                           const char *predicate,
                           const QString &object)
{
    QString obj = object.isEmpty() ? nextVariable() : object;

    deletions << LAT("DELETE {")
            << encloseUrl(subject)
            << LAT(predicate) << obj
            << LAT("} WHERE {")
            << encloseUrl(subject) << LAT(predicate) << obj
            << LAT("}");
}

void UpdateQuery::resourceDeletion(const QUrl &subject,
                                   const char *predicate)
{
    QString r = nextVariable();
    deletions << LAT("DELETE {")
            << encloseUrl(subject)
            << LAT(predicate) << r << LAT(".")
            << r << LAT("rdf:type rdfs:Resource .")
            << LAT("} WHERE {")
            << encloseUrl(subject) << LAT(predicate) << r
            << LAT("} ");
}

void UpdateQuery::deletion(const QString &query) {
    deletions << query;
}


void UpdateQuery::insertionRaw(const QUrl &subject,
                               const char *predicate,
                               const QString &object,
                               bool modify) {
    if (modify)
        deletion(subject, predicate);

    insertions.insertMulti(subject, LAT(predicate) % LAT(" ") % object);
}

void UpdateQuery::insertion(const QUrl &subject,
                            const char *predicate,
                            const QString &object,
                            bool modify) {
    QSparqlBinding objectBinding;
    objectBinding.setValue(object);
    insertionRaw(subject, predicate, objectBinding.toString(), modify);
}

void UpdateQuery::insertion(const QUrl &subject,
                            const char *predicate,
                            const QDateTime &object,
                            bool modify) {
    insertionRaw(subject,
                 predicate,
                 LAT("\"") % object.toUTC().toString(Qt::ISODate) % LAT("Z\"^^xsd:dateTime"),
                 modify);
}

void UpdateQuery::insertion(const QUrl &subject,
                            const char *predicate,
                            bool object,
                            bool modify) {
    insertionRaw(subject,
                 predicate,
                 object ? LAT("true") : LAT("false"),
                 modify);
}

void UpdateQuery::insertion(const QUrl &subject,
                            const char *predicate,
                            int object,
                            bool modify) {
    insertionRaw(subject,
                 predicate,
                 LAT("\"") % QString::number(object) % LAT("\"^^xsd:int"),
                 modify);
}


void UpdateQuery::insertion(const QUrl &subject,
                            const char *predicate,
                            const QUrl &object,
                            bool modify) {
    insertionRaw(subject,
                 predicate,
                 encloseUrl(object),
                 modify);
}

void UpdateQuery::insertion(const QString &statement)
{
    insertions.insertMulti(QUrl(), statement);
}

void UpdateQuery::insertionSilent(const QString &statement)
{
    silents << statement;
}

void UpdateQuery::appendInsertion(const QString &statement)
{
    postInsertions << statement;
}

QString UpdateQuery::query()
{
    QStringList query;
    query << deletions;

    if (!silents.isEmpty())
        query << LAT("INSERT SILENT {")
              << silents
              << LAT("}");

    if (!insertions.isEmpty()) {
        query << LAT("INSERT {");
        copyReverse(insertions.values(QUrl()), query);

        foreach (QUrl subject, insertions.uniqueKeys()) {
            if (subject.isEmpty())
                continue;

            query << encloseUrl(subject);
            copyReverse(insertions.values(subject), query, LAT("; "));
            query << LAT(" .");
        }
        query << LAT("}");
    }

    query << postInsertions;

    return query.join(LAT(" "));
}

QString UpdateQuery::nextVariable()
{
    return LAT("?_") % QString::number(nextVar++);
}

} //namespace

