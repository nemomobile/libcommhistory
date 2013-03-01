#include "callproxymodel.h"
#include "event.h"

CallProxyModel::CallProxyModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_source(new CommHistory::CallModel(CommHistory::CallModel::SortByTime, this)),
    m_grouping(GroupByNone),
    m_componentComplete(false)
{
    m_source->setQueryMode(CommHistory::EventModel::AsyncQuery);
    m_source->setTreeMode(true);

    this->setSourceModel(m_source);
    this->setDynamicSortFilter(true);

    this->setRoleNames(m_source->roleNames());
}

void CallProxyModel::classBegin()
{
}

void CallProxyModel::componentComplete()
{
    m_componentComplete = true;

    if(!m_source->getEvents()) {
        qWarning() << "getEvents() failed on CommHistory::CallModel";
        return;
    }
}

CallProxyModel::GroupBy CallProxyModel::groupBy() const
{
    return m_grouping;
}

void CallProxyModel::setGroupBy(GroupBy grouping)
{
    if (m_grouping != grouping) {
        m_grouping = grouping;
        m_source->setFilter(CommHistory::CallModel::Sorting(grouping));

        emit groupByChanged();
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

bool CallProxyModel::markAllRead()
{
    return m_source->markAllRead();
}
