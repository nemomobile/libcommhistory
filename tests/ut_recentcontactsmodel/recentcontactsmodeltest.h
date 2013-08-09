#ifndef RECENTCONTACTSMODELTEST_H
#define RECENTCONTACTSMODELTEST_H

#include <QObject>

class RecentContactsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void simple();
    void limitedFill();
    void repeated();
    void limitedDynamic();
    void differentTypes();
    void selectionProperty();
    void contactRemoved();

    void cleanup();
    void cleanupTestCase();
};

#endif
