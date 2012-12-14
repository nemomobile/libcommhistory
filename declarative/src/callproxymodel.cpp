#include "callproxymodel.h"
#include "event.h"

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
    QSortFilterProxyModel::setFilterKeyColumn(role - CommHistory::EventModel::BaseRole);
    QSortFilterProxyModel::setFilterRole(role);
}

void CallProxyModel::deleteAt(int index)
{
    QModelIndex sourceIndex = mapToSource(CallProxyModel::index(index, 0));
    CommHistory::Event event = m_source->event(sourceIndex);

    if (event.isValid())
        m_source->deleteEvent(event);
}
