/***************************************************************************
                          equitiesmodel.h
                             -------------------
    copyright            : (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EQUITIESMODEL_H
#define EQUITIESMODEL_H

// ----------------------------------------------------------------------------
// QT Includes

#include <QStandardItemModel>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KItemModels/KRecursiveFilterProxyModel>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyenums.h"

class MyMoneyObject;
class MyMoneyAccount;
class EquitiesModel : public QStandardItemModel
{
  Q_OBJECT

public:
  enum Column { Equity = 0, Symbol, Value, Quantity, Price };
  enum Role { InvestmentID = Qt::UserRole, EquityID = Qt::UserRole, SecurityID = Qt::UserRole + 1 };

  ~EquitiesModel();

  auto getColumns();
  static QString getHeaderName(const Column column);

public slots:
  void slotObjectAdded(eMyMoney::File::Object objType, const MyMoneyObject * const obj);
  void slotObjectModified(eMyMoney::File::Object objType, const MyMoneyObject * const obj);
  void slotObjectRemoved(eMyMoney::File::Object objType, const QString& id);
  void slotBalanceOrValueChanged(const MyMoneyAccount &account);

private:
  EquitiesModel(QObject *parent = nullptr);
  EquitiesModel(const EquitiesModel&);
  EquitiesModel& operator=(EquitiesModel&);
  friend class Models;  // only this class can create EquitiesModel

  void init();
  void load();

protected:
  class Private;
  Private* const d;
};

class EquitiesFilterProxyModel : public KRecursiveFilterProxyModel
{
  Q_OBJECT

public:
  EquitiesFilterProxyModel(QObject *parent , EquitiesModel *model, const QList<EquitiesModel::Column> &columns = QList<EquitiesModel::Column>());
  ~EquitiesFilterProxyModel();

  QList<EquitiesModel::Column> &getVisibleColumns();
  void setHideClosedAccounts(const bool hideClosedAccounts);
  void setHideZeroBalanceAccounts(const bool hideZeroBalanceAccounts);

signals:
  void columnToggled(const EquitiesModel::Column column, const bool show);

public slots:
  void slotColumnsMenu(const QPoint);

protected:
  bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  class Private;
  Private* const d;
};

#endif // EQUITIESMODEL_H
