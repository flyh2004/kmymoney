/***************************************************************************
                             groupmarkers.h
                             ----------
    begin                : Fri Mar 10 2006
    copyright            : (C) 2006 by Thomas Baumgart
    email                : Thomas Baumgart <ipwizard@users.sourceforge.net>
                           (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef GROUPMARKERS_H
#define GROUPMARKERS_H

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "groupmarker.h"

namespace eWidgets { enum class SortField;
                     namespace Transaction { enum class Column; }
                     namespace Register { enum class DetailColumn;} }
namespace eMyMoney { enum class Account; }

namespace KMyMoneyRegister
{
  class RegisterItem;

  class TypeGroupMarkerPrivate;
  class TypeGroupMarker : public GroupMarker
  {
    Q_DISABLE_COPY(TypeGroupMarker)

  public:
    explicit TypeGroupMarker(Register* getParent, eWidgets::eRegister::CashFlowDirection dir, eMyMoney::Account accType);
    ~TypeGroupMarker() override;

    eWidgets::eRegister::CashFlowDirection sortType() const override;

  private:
    Q_DECLARE_PRIVATE(TypeGroupMarker)
  };

  class PayeeGroupMarker : public GroupMarker
  {
    Q_DISABLE_COPY(PayeeGroupMarker)

  public:
    explicit PayeeGroupMarker(Register* getParent, const QString& name);
    ~PayeeGroupMarker() override;

    const QString& sortPayee() const override;
  };

  class CategoryGroupMarker : public GroupMarker
  {
    Q_DISABLE_COPY(CategoryGroupMarker)

  public:
    explicit CategoryGroupMarker(Register* getParent, const QString& category);
    ~CategoryGroupMarker() override;

    const QString& sortCategory() const override;
    const QString sortSecurity() const override;
    const char* className() override;
  };

  class ReconcileGroupMarkerPrivate;
  class ReconcileGroupMarker : public GroupMarker
  {
    Q_DISABLE_COPY(ReconcileGroupMarker)

  public:
    explicit ReconcileGroupMarker(Register* getParent, eMyMoney::Split::State state);
    ~ReconcileGroupMarker() override;

    eMyMoney::Split::State sortReconcileState() const override;

  private:
    Q_DECLARE_PRIVATE(ReconcileGroupMarker)
  };

} // namespace

#endif
