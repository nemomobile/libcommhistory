#ifndef SINGLEEVENTMODELTEST_H
#define SINGLEEVENTMODELTEST_H

#include <QObject>

class SingleEventModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void getEventByUri();
    void getEventByTokens();
    void cleanupTestCase();
};

#endif
