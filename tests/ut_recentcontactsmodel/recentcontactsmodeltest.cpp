#include <QtTest/QtTest>

#include <time.h>
#include "recentcontactsmodeltest.h"
#include "recentcontactsmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"

#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
ModelWatcher watcher;

int aliceId, bobId, charlieId;
const QString aliceName("Alice");
const QString alicePhone1("1234567");
const QString alicePhone2("2345678");
const QString bobName("Bob");
const QString bobPhone("9876543");
const QPair<QString, QString> bobIm(qMakePair(ACCOUNT1, QString::fromLatin1("bob@example.org")));
const QString charlieName("Charlie");
const QPair<QString, QString> charlieIm1(qMakePair(ACCOUNT1, QString::fromLatin1("charlie@example.org")));
const QPair<QString, QString> charlieIm2(qMakePair(ACCOUNT1, QString::fromLatin1("douglas@example.org")));
const QString phoneAccount(RING_ACCOUNT);

typedef QPair<int, QString> ContactDetails;

static void addEvents(int from, int to)
{
    EventModel eventsModel;
    watcher.setModel(&eventsModel);

    if (to >= 1 && from <= 1) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), alicePhone1);
    }
    if (to >= 2 && from <= 2) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::CallEvent, Event::Outbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), bobPhone);
    }
    if (to >= 3 && from <= 3) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::IMEvent, Event::Inbound, charlieIm1.first, group1.id(), "", false, false, QDateTime::currentDateTime(), charlieIm1.second);
    }
    if (to >= 4 && from <= 4) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::SMSEvent, Event::Outbound, phoneAccount, group1.id(), "", false, false, QDateTime::currentDateTime(), alicePhone2);
    }
    if (to >= 5 && from <= 5) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), bobPhone);
    }
    if (to >= 6 && from <= 6) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::IMEvent, Event::Outbound, charlieIm2.first, group1.id(), "", false, false, QDateTime::currentDateTime(), charlieIm2.second);
    }
    if (to >= 7 && from <= 7) {
        QTest::qWait(1000);
        addTestEvent(eventsModel, Event::IMEvent, Event::Outbound, bobIm.first, group1.id(), "", false, false, QDateTime::currentDateTime(), bobIm.second);
    }

    int count = to - from + 1;
    QVERIFY(watcher.waitForAdded(count, count));
}

static void addEvents(int to)
{
    addEvents(1, to);
}

class InsertionSpy : public QObject
{
    Q_OBJECT

public:
    InsertionSpy(QAbstractItemModel &m, QObject *parent = 0)
        : QObject(parent),
          model(m),
          insertionCount(0)
    {
        QObject::connect(&model, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                         this, SLOT(rowsInserted(const QModelIndex &, int, int)));
    }

    int count() const { return insertionCount; }

private Q_SLOTS:
    void rowsInserted(const QModelIndex &, int start, int end) { insertionCount += (end - start + 1); }

private:
    QAbstractItemModel &model;
    int insertionCount;
};

class RemovalSpy : public QObject
{
    Q_OBJECT

public:
    RemovalSpy(QAbstractItemModel &m, QObject *parent = 0)
        : QObject(parent),
          model(m),
          removalCount(0)
    {
        QObject::connect(&model, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
                         this, SLOT(rowsRemoved(const QModelIndex &, int, int)));
    }

    int count() const { return removalCount; }

private Q_SLOTS:
    void rowsRemoved(const QModelIndex &, int start, int end) { removalCount += (end - start + 1); }

private:
    QAbstractItemModel &model;
    int removalCount;
};

void RecentContactsModelTest::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    deleteAll();

    qsrand(QDateTime::currentDateTime().toTime_t());

    addTestGroups( group1, group2 );

    aliceId = addTestContact(aliceName, alicePhone1);
    QVERIFY(aliceId != -1);
    QVERIFY(addTestContactAddress(aliceId, alicePhone2));

    bobId = addTestContact(bobName, bobPhone);
    QVERIFY(bobId != -1);
    QVERIFY(addTestContactAddress(bobId, bobIm.second, bobIm.first));

    charlieId = addTestContact(charlieName, charlieIm1.second, charlieIm1.first);
    QVERIFY(charlieId != -1);
    QVERIFY(addTestContactAddress(charlieId, charlieIm2.second, charlieIm2.first));
}

void RecentContactsModelTest::init()
{
    // Re-add the groups which are purged when all events are removed
    addTestGroups( group1, group2 );
}

void RecentContactsModelTest::simple()
{
    addEvents(3);

    RecentContactsModel model;

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
}

void RecentContactsModelTest::limitedFill()
{
    addEvents(3);

    RecentContactsModel model;
    model.setLimit(2);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(insert.count(), 2);

    // We should have one row for each contact, limited to 2
    QCOMPARE(model.rowCount(), 2);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
}

void RecentContactsModelTest::repeated()
{
    addEvents(5);

    RecentContactsModel model;
    model.setLimit(3);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::CallEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), bobPhone);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), alicePhone2);

    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.localUid(), charlieIm1.first);
    QCOMPARE(e.remoteUid(), charlieIm1.second);
}

void RecentContactsModelTest::limitedDynamic()
{
    addEvents(2);

    RecentContactsModel model;
    model.setLimit(2);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(insert.count(), 2);

    // We should have one row for each contact, limited to 2
    QCOMPARE(model.rowCount(), 2);

    Event e;

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Add more events; we have to wait for each one to be sure of when we're done
    for (int count = 3; count <= 5; ++count) {
        addEvents(count, count);
        QTRY_COMPARE(insert.count(), count);
    }

    QTRY_COMPARE(insert.count(), 5);
    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::CallEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), bobPhone);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), alicePhone2);
}

void RecentContactsModelTest::differentTypes()
{
    RecentContactsModel model;
    model.setLimit(3);

    QVERIFY(model.getEvents());
    QCOMPARE(model.rowCount(), 0);

    InsertionSpy insert(model);

    for (int count = 1; count <= 7; ++count) {
        addEvents(count, count);
        QTRY_COMPARE(insert.count(), count);
    }

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), bobIm.first);
    QCOMPARE(e.remoteUid(), bobIm.second);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), charlieIm2.first);
    QCOMPARE(e.remoteUid(), charlieIm2.second);

    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), alicePhone2);
}

void RecentContactsModelTest::selectionProperty()
{
    RecentContactsModel phoneModel;
    phoneModel.setSelectionProperty(QString::fromLatin1("phoneNumber"));

    RecentContactsModel imModel;
    imModel.setSelectionProperty(QString::fromLatin1("accountUri"));

    RecentContactsModel emailModel;
    emailModel.setSelectionProperty(QString::fromLatin1("emailAddress"));

    QVERIFY(phoneModel.getEvents());
    QVERIFY(imModel.getEvents());
    QVERIFY(emailModel.getEvents());

    QCOMPARE(phoneModel.rowCount(), 0);
    QCOMPARE(imModel.rowCount(), 0);
    QCOMPARE(emailModel.rowCount(), 0);

    for (int count = 1; count <= 7; ++count) {
        addEvents(count, count);
        QTest::qWait(1000);
    }

    // Two contacts have phone numbers, two have IM addresses, none have email
    QCOMPARE(phoneModel.rowCount(), 2);
    QCOMPARE(imModel.rowCount(), 2);
    QCOMPARE(emailModel.rowCount(), 0);

    Event e;

    // Phone contacts (not events)
    e = phoneModel.event(phoneModel.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), bobIm.first);
    QCOMPARE(e.remoteUid(), bobIm.second);

    e = phoneModel.event(phoneModel.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), phoneAccount);
    QCOMPARE(e.remoteUid(), alicePhone2);

    // IM contacts
    e = imModel.event(imModel.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), bobIm.first);
    QCOMPARE(e.remoteUid(), bobIm.second);

    e = imModel.event(imModel.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.localUid(), charlieIm2.first);
    QCOMPARE(e.remoteUid(), charlieIm2.second);

    {
        // Repeat the tests to test filtering on fill
        RecentContactsModel phoneModel;
        phoneModel.setSelectionProperty(QString::fromLatin1("phoneNumber"));

        RecentContactsModel imModel;
        imModel.setSelectionProperty(QString::fromLatin1("accountUri"));

        RecentContactsModel emailModel;
        emailModel.setSelectionProperty(QString::fromLatin1("emailAddress"));

        QVERIFY(phoneModel.getEvents());
        QVERIFY(imModel.getEvents());
        QVERIFY(emailModel.getEvents());

        QTRY_COMPARE(phoneModel.rowCount(), 2);
        QTRY_COMPARE(imModel.rowCount(), 2);
        QCOMPARE(emailModel.rowCount(), 0);

        // Results should be indentical
        e = phoneModel.event(phoneModel.index(0, 0));
        QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
        QCOMPARE(e.type(), Event::IMEvent);
        QCOMPARE(e.direction(), Event::Outbound);
        QCOMPARE(e.localUid(), bobIm.first);
        QCOMPARE(e.remoteUid(), bobIm.second);

        e = phoneModel.event(phoneModel.index(1, 0));
        QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
        QCOMPARE(e.type(), Event::SMSEvent);
        QCOMPARE(e.direction(), Event::Outbound);
        QCOMPARE(e.localUid(), phoneAccount);
        QCOMPARE(e.remoteUid(), alicePhone2);

        e = imModel.event(imModel.index(0, 0));
        QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
        QCOMPARE(e.type(), Event::IMEvent);
        QCOMPARE(e.direction(), Event::Outbound);
        QCOMPARE(e.localUid(), bobIm.first);
        QCOMPARE(e.remoteUid(), bobIm.second);

        e = imModel.event(imModel.index(1, 0));
        QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
        QCOMPARE(e.type(), Event::IMEvent);
        QCOMPARE(e.direction(), Event::Outbound);
        QCOMPARE(e.localUid(), charlieIm2.first);
        QCOMPARE(e.remoteUid(), charlieIm2.second);
    }
}

void RecentContactsModelTest::contactRemoved()
{
    QString dougalName("Dougal");
    QString dougalPhone("5550000");
    int dougalId = addTestContact(dougalName, dougalPhone);
    QVERIFY(dougalId != -1);

    addEvents(2);

    // Add an event for the new contact
    EventModel eventsModel;
    QTest::qWait(1000);
    addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), dougalPhone);

    RecentContactsModel model;
    InsertionSpy insert(model);
    RemovalSpy remove(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(dougalId, dougalName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    QCOMPARE(remove.count(), 0);
    deleteTestContact(dougalId);

    // The removed contact's event should be removed
    QTRY_COMPARE(remove.count(), 1);

    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
}

void RecentContactsModelTest::cleanup()
{
    cleanupTestEvents();
    cleanupTestGroups();
}

void RecentContactsModelTest::cleanupTestCase()
{
    deleteAll();
}

QTEST_MAIN(RecentContactsModelTest)

#include "recentcontactsmodeltest.moc"
