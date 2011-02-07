#ifndef EVENTSQUERYTEST_H
#define EVENTSQUERYTEST_H

#include <QObject>

class QSparqlConnection;

class EventsQueryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void empty();
    void all();
    void plain();
    void tofrom_data();
    void tofrom();
    void distinct();

private:
    QSparqlConnection *conn;
};

#endif // EVENTSQUERYTEST_H
