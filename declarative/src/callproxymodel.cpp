#include "callproxymodel.h"

CallProxyModel::CallProxyModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_source(new CommHistory::CallModel(CommHistory::CallModel::SortByTime, this))
{
    m_source->setQueryMode(CommHistory::EventModel::AsyncQuery);

    this->setSourceModel(m_source);
    this->setDynamicSortFilter(true);

    if(!m_source->getEvents())
    {
        qWarning() << "getEvents() failed on CommHistory::CallModel";
        return;
    }
}

QVariant CallProxyModel::data(const QModelIndex &index, int role) const
{
    // Reformat role to access column members by subtracting UserRole offset.
    return QSortFilterProxyModel::data(this->index(index.row(), role - Qt::UserRole), role);
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
