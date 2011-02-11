#include <QtTest/QtTest>
#include <QRegExp>

#include <QSparqlConnection>
#include <QSparqlQuery>
#include <QSparqlResult>

#include "eventsquery.h"

#include "eventsquerytest.h"

using namespace CommHistory;

void EventsQueryTest::initTestCase()
{
    conn = new QSparqlConnection(QLatin1String("QTRACKER_DIRECT"));
}

void EventsQueryTest::cleanupTestCase()
{
    delete conn;
}

void EventsQueryTest::empty()
{
    Event::PropertySet props;

    EventsQuery q(props);

    QString query = q.query();
    qDebug() << query;

    QVERIFY(q.eventProperties().contains(Event::Id));

    QCOMPARE(QRegExp("^SELECT\\s+\\?message\\b\\s+WHERE\\s*\\{\\s*\\?message\\s+a\\s+nmo:Message\\s*\\.?\\s*\\}\\s*$").indexIn(query), 0);

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

void EventsQueryTest::all()
{
    EventsQuery q(Event::allProperties());

    QString query = q.query();
    qDebug() << query;

    QVERIFY(q.eventProperties().contains(Event::Id));

    QVERIFY(!q.query().isEmpty());

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

void EventsQueryTest::plain()
{
    Event::PropertySet props;
    props << Event::StartTime
          << Event::Status;

    EventsQuery q(props);

    q.addPattern("%1 rdf:type nmo:SMSMessage .").variable(Event::Id);
    q.addModifier("ORDER BY DESC(%1)").variable(Event::StartTime);

    QString query = q.query();
    qDebug() << query;

    QVERIFY(q.eventProperties().contains(Event::Id));

    QCOMPARE(QRegExp("^SELECT\\s+\\?message\\b.+\\bWHERE.+\\?message\\s+rdf:type nmo:SMSMessage\\b.+ORDER BY DESC\\(.+\\)$").indexIn(query), 0);

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

void EventsQueryTest::tofrom_data()
{
    QTest::addColumn<int>("prop1");
    QTest::addColumn<int>("prop2");

    QTest::newRow("local uid") << (int)Event::LocalUid << -1;
    QTest::newRow("remote uid") << (int)Event::RemoteUid << -1;
    QTest::newRow("remote/local uid") << (int)Event::RemoteUid << (int)Event::LocalUid;
}

void EventsQueryTest::tofrom()
{
    QFETCH(int, prop1);
    QFETCH(int, prop2);

    Event::PropertySet props;
    props << (Event::Property)prop1;
    if (prop2 >= 0)
        props << (Event::Property)prop2;

    EventsQuery q(props);

    QString query = q.query();
    qDebug() << query;

    QVERIFY(q.eventProperties().contains(Event::Id));
    QVERIFY(q.eventProperties().contains(Event::LocalUid));
    QVERIFY(q.eventProperties().contains(Event::RemoteUid));
    QVERIFY(q.eventProperties().contains(Event::Direction));

    QCOMPARE(QRegExp("^SELECT.+\\bnmo:isSent\\(\\?message\\)\\s+.*\\bWHERE").indexIn(query), 0);

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

void EventsQueryTest::distinct()
{
    Event::PropertySet props;

    EventsQuery q(props);

    QCOMPARE(q.query().indexOf("DISTINCT"), -1);

    q.setDistinct(true);

    QString query = q.query();
    qDebug() << query;

    QCOMPARE(QRegExp("^SELECT\\s+DISTINCT\\b").indexIn(query), 0);

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

void EventsQueryTest::contact()
{
    Event::PropertySet props;
    props << Event::ContactId;

    EventsQuery q(props);

    QString query = q.query();
    qDebug() << query;

    QVERIFY(q.eventProperties().contains(Event::Id));
    //QVERIFY(q.eventProperties().contains(Event::Type));
    QVERIFY(q.eventProperties().contains(Event::ContactId));
    QVERIFY(q.eventProperties().contains(Event::ContactName));

    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    QVERIFY(!result->hasError());
}

QTEST_MAIN(EventsQueryTest)
