/*
 * This file is part of KMyMoney, A Personal Finance Manager for KDE
 * Copyright (C) 2014 Christian Dávid <christian-david@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NATIONALACCOUNTEDIT_H
#define NATIONALACCOUNTEDIT_H

#include <QWidget>

namespace Ui
{
  class nationalAccountEdit;
}

class nationalAccountEdit : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(QString accountNumber READ accountNumber WRITE setAccountNumber NOTIFY accountNumberChannged STORED true DESIGNABLE true)
  Q_PROPERTY(QString institutionCode READ institutionCode WRITE setInstitutionCode NOTIFY institutionCodeChanged STORED true DESIGNABLE true)

public:
  nationalAccountEdit(QWidget* parent = 0);

  QString accountNumber() const;
  QString institutionCode() const;

public slots:
  void setAccountNumber( const QString& );
  void setInstitutionCode( const QString& );

signals:
  void institutionCodeChanged( QString );
  void accountNumberChannged( QString );

private:
  Ui::nationalAccountEdit* ui;
};

#endif // NATIONALACCOUNTEDIT_H
