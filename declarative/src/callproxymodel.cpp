#include "callproxymodel.h"

CallProxyModel::CallProxyModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_source(new CommHistory::CallModel(CommHistory::CallModel::SortByTime, this))
{
    m_source->setQueryMode(CommHistory::EventModel::AsyncQuery);

    this->setSourceModel(m_source);
    this->setDynamicSortFilter(true);

    this->setRoleNames(m_source->roleNames());

    if(!m_source->getEvents())
    {
        qWarning() << "getEvents() failed on CommHistory::CallModel";
        return;
    }
}

void CallProxyModel::setSortRole(int role)
{
    QSortFilterProxyModel::setSortRole(role);
}

void CallProxyModel::setFilterRole(int role)
{
    QSortFilterProxyModel::setFilterKeyColumn(role - Qt::UserRole);
    QSortFilterProxyModel::setFilterRole(role);
}
