/***************************************************************************
                          objectinfotable.cpp
                         -------------------
    begin                : Sat 28 jun 2008
    copyright            : (C) 2004-2005 by Ace Jones <acejones@users.sourceforge.net>
                               2008 by Alvaro Soliverez <asoliverez@gmail.com>

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "objectinfotable.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QList>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyfile.h"
#include "mymoneyinstitution.h"
#include "mymoneyaccount.h"
#include "mymoneyaccountloan.h"
#include "mymoneysecurity.h"
#include "mymoneyprice.h"
#include "mymoneypayee.h"
#include "mymoneysplit.h"
#include "mymoneytransaction.h"
#include "mymoneyreport.h"
#include "mymoneyschedule.h"
#include "mymoneyexception.h"
#include "kmymoneyutils.h"
#include "reportaccount.h"

namespace reports
{

// ****************************************************************************
//
// ObjectInfoTable implementation
//
// ****************************************************************************

/**
  * TODO
  *
  * - Collapse 2- & 3- groups when they are identical
  * - Way more test cases (especially splits & transfers)
  * - Option to collapse splits
  * - Option to exclude transfers
  *
  */

ObjectInfoTable::ObjectInfoTable(const MyMoneyReport& _report): ListTable(_report)
{
  // separated into its own method to allow debugging (setting breakpoints
  // directly in ctors somehow does not work for me (ipwizard))
  // TODO: remove the init() method and move the code back to the ctor
  init();
}

void ObjectInfoTable::init()
{
  m_columns.clear();
  m_group.clear();
  m_subtotal.clear();
  switch (m_config.rowType()) {
    case MyMoneyReport::eSchedule:
      constructScheduleTable();
      m_columns << ctNextDueDate << ctName;
      break;
    case MyMoneyReport::eAccountInfo:
      constructAccountTable();
      m_columns << ctInstitution << ctType << ctName;
      break;
    case MyMoneyReport::eAccountLoanInfo:
      constructAccountLoanTable();
      m_columns << ctInstitution << ctType << ctName;
      break;
    default:
      break;
  }

  // Sort the data to match the report definition
  m_subtotal << ctValue;

  switch (m_config.rowType()) {
    case MyMoneyReport::eSchedule:
      m_group << ctType;
      m_subtotal << ctValue;
      break;
    case MyMoneyReport::eAccountInfo:
    case MyMoneyReport::eAccountLoanInfo:
      m_group << ctTopCategory << ctInstitution;
      m_subtotal << ctCurrentBalance;
      break;
    default:
      throw MYMONEYEXCEPTION("ObjectInfoTable::ObjectInfoTable(): unhandled row type");
  }

  QVector<cellTypeE> sort = QVector<cellTypeE>::fromList(m_group) << QVector<cellTypeE>::fromList(m_columns) << ctID << ctRank;

  switch (m_config.rowType()) {
    case MyMoneyReport::eSchedule:
      if (m_config.detailLevel() == MyMoneyReport::eDetailAll) {
        m_columns << ctName << ctPayee << ctPaymentType << ctOccurence
                  << ctNextDueDate << ctCategory; // krazy:exclude=spelling
      } else {
        m_columns << ctName << ctPayee << ctPaymentType << ctOccurence
                  << ctNextDueDate; // krazy:exclude=spelling
      }
      break;
    case MyMoneyReport::eAccountInfo:
      m_columns << ctType << ctName << ctNumber << ctDescription
                << ctOpeningDate << ctCurrencyName << ctBalanceWarning
                << ctCreditWarning << ctMaxCreditLimit
                << ctTax << ctFavorite;
      break;
    case MyMoneyReport::eAccountLoanInfo:
      m_columns << ctType << ctName << ctNumber << ctDescription
                << ctOpeningDate << ctCurrencyName << ctPayee
                << ctLoanAmount << ctInterestRate << ctNextInterestChange
                << ctPeriodicPayment << ctFinalPayment << ctFavorite;
      break;
    default:
      m_columns.clear();
  }

  TableRow::setSortCriteria(sort);
  qSort(m_rows);
}

void ObjectInfoTable::constructScheduleTable()
{
  MyMoneyFile* file = MyMoneyFile::instance();

  QList<MyMoneySchedule> schedules;

  schedules = file->scheduleList(QString(), eMyMoney::Schedule::Type::Any, eMyMoney::Schedule::Occurrence::Any, eMyMoney::Schedule::PaymentType::Any, m_config.fromDate(), m_config.toDate(), false);

  QList<MyMoneySchedule>::const_iterator it_schedule = schedules.constBegin();
  while (it_schedule != schedules.constEnd()) {
    MyMoneySchedule schedule = *it_schedule;

    ReportAccount account = schedule.account();

    if (m_config.includes(account))  {
      //get fraction for account
      int fraction = account.fraction();

      //use base currency fraction if not initialized
      if (fraction == -1)
        fraction = MyMoneyFile::instance()->baseCurrency().smallestAccountFraction();

      TableRow scheduleRow;

      //convert to base currency if needed
      MyMoneyMoney xr = MyMoneyMoney::ONE;
      if (m_config.isConvertCurrency() && account.isForeignCurrency()) {
        xr = account.baseCurrencyPrice(QDate::currentDate()).reduce();
      }

      // help for sort and render functions
      scheduleRow[ctRank] = QLatin1Char('0');

      //schedule data
      scheduleRow[ctID] = schedule.id();
      scheduleRow[ctName] = schedule.name();
      scheduleRow[ctNextDueDate] = schedule.nextDueDate().toString(Qt::ISODate);
      scheduleRow[ctType] = KMyMoneyUtils::scheduleTypeToString(schedule.type());
      scheduleRow[ctOccurence] = i18nc("Frequency of schedule", schedule.occurrenceToString().toLatin1()); // krazy:exclude=spelling
      scheduleRow[ctPaymentType] = KMyMoneyUtils::paymentMethodToString(schedule.paymentType());

      //scheduleRow["category"] = account.name();

      //to get the payee we must look into the splits of the transaction
      MyMoneyTransaction transaction = schedule.transaction();
      MyMoneySplit split = transaction.splitByAccount(account.id(), true);
      scheduleRow[ctValue] = (split.value() * xr).toString();
      MyMoneyPayee payee = file->payee(split.payeeId());
      scheduleRow[ctPayee] = payee.name();
      m_rows += scheduleRow;

      //the text matches the main split
      bool transaction_text = m_config.match(&split);

      if (m_config.detailLevel() == MyMoneyReport::eDetailAll) {
        //get the information for all splits
        QList<MyMoneySplit> splits = transaction.splits();
        QList<MyMoneySplit>::const_iterator split_it = splits.constBegin();
        for (; split_it != splits.constEnd(); ++split_it) {
          TableRow splitRow;
          ReportAccount splitAcc = (*split_it).accountId();

          splitRow[ctRank] = QLatin1Char('1');
          splitRow[ctID] = schedule.id();
          splitRow[ctName] = schedule.name();
          splitRow[ctType] = KMyMoneyUtils::scheduleTypeToString(schedule.type());
          splitRow[ctNextDueDate] = schedule.nextDueDate().toString(Qt::ISODate);

          if ((*split_it).value() == MyMoneyMoney::autoCalc) {
            splitRow[ctSplit] = MyMoneyMoney::autoCalc.toString();
          } else if (! splitAcc.isIncomeExpense()) {
            splitRow[ctSplit] = (*split_it).value().toString();
          } else {
            splitRow[ctSplit] = (- (*split_it).value()).toString();
          }

          //if it is an assett account, mark it as a transfer
          if (! splitAcc.isIncomeExpense()) {
            splitRow[ctCategory] = ((* split_it).value().isNegative())
                                   ? i18n("Transfer from %1" , splitAcc.fullName())
                                   : i18n("Transfer to %1" , splitAcc.fullName());
          } else {
            splitRow [ctCategory] = splitAcc.fullName();
          }

          //add the split only if it matches the text or it matches the main split
          if (m_config.match(&(*split_it))
              || transaction_text)
            m_rows += splitRow;
        }
      }
    }
    ++it_schedule;
  }
}

void ObjectInfoTable::constructAccountTable()
{
  MyMoneyFile* file = MyMoneyFile::instance();

  //make sure we have all subaccounts of investment accounts
  includeInvestmentSubAccounts();

  QList<MyMoneyAccount> accounts;
  file->accountList(accounts);
  QList<MyMoneyAccount>::const_iterator it_account = accounts.constBegin();
  while (it_account != accounts.constEnd()) {
    TableRow accountRow;
    ReportAccount account = *it_account;

    if (m_config.includes(account)
        && account.accountType() != eMyMoney::Account::Stock
        && !account.isClosed()) {
      MyMoneyMoney value;
      accountRow[ctRank] = QLatin1Char('0');
      accountRow[ctTopCategory] = MyMoneyAccount::accountTypeToString(account.accountGroup());
      accountRow[ctInstitution] = (file->institution(account.institutionId())).name();
      accountRow[ctType] = MyMoneyAccount::accountTypeToString(account.accountType());
      accountRow[ctName] = account.name();
      accountRow[ctNumber] = account.number();
      accountRow[ctDescription] = account.description();
      accountRow[ctOpeningDate] = account.openingDate().toString(Qt::ISODate);
      //accountRow["currency"] = (file->currency(account.currencyId())).tradingSymbol();
      accountRow[ctCurrencyName] = (file->currency(account.currencyId())).name();
      accountRow[ctBalanceWarning] = account.value("minBalanceEarly");
      accountRow[ctMaxBalanceLimit] = account.value("minBalanceAbsolute");
      accountRow[ctCreditWarning] = account.value("maxCreditEarly");
      accountRow[ctMaxCreditLimit] = account.value("maxCreditAbsolute");
      accountRow[ctTax] = account.value("Tax") == QLatin1String("Yes") ? i18nc("Is this a tax account?", "Yes") : QString();
      accountRow[ctOpeningBalance] = account.value("OpeningBalanceAccount") == QLatin1String("Yes") ? i18nc("Is this an opening balance account?", "Yes") : QString();
      accountRow[ctFavorite] = account.value("PreferredAccount") == QLatin1String("Yes") ? i18nc("Is this a favorite account?", "Yes") : QString();

      //investment accounts show the balances of all its subaccounts
      if (account.accountType() == eMyMoney::Account::Investment) {
        value = investmentBalance(account);
      } else {
        value = file->balance(account.id());
      }

      //convert to base currency if needed
      if (m_config.isConvertCurrency() && account.isForeignCurrency()) {
        MyMoneyMoney xr = account.baseCurrencyPrice(QDate::currentDate()).reduce();
        value = value * xr;
      }
      accountRow[ctCurrentBalance] = value.toString();

      m_rows += accountRow;
    }
    ++it_account;
  }
}

void ObjectInfoTable::constructAccountLoanTable()
{
  MyMoneyFile* file = MyMoneyFile::instance();

  QList<MyMoneyAccount> accounts;
  file->accountList(accounts);
  QList<MyMoneyAccount>::const_iterator it_account = accounts.constBegin();
  while (it_account != accounts.constEnd()) {
    TableRow accountRow;
    ReportAccount account = *it_account;
    MyMoneyAccountLoan loan = *it_account;

    if (m_config.includes(account) && account.isLoan() && !account.isClosed()) {
      //convert to base currency if needed
      MyMoneyMoney xr = MyMoneyMoney::ONE;
      if (m_config.isConvertCurrency() && account.isForeignCurrency()) {
        xr = account.baseCurrencyPrice(QDate::currentDate()).reduce();
      }

      accountRow[ctRank] = QLatin1Char('0');
      accountRow[ctTopCategory] = MyMoneyAccount::accountTypeToString(account.accountGroup());
      accountRow[ctInstitution] = (file->institution(account.institutionId())).name();
      accountRow[ctType] = MyMoneyAccount::accountTypeToString(account.accountType());
      accountRow[ctName] = account.name();
      accountRow[ctNumber] = account.number();
      accountRow[ctDescription] = account.description();
      accountRow[ctOpeningDate] = account.openingDate().toString(Qt::ISODate);
      //accountRow["currency"] = (file->currency(account.currencyId())).tradingSymbol();
      accountRow[ctCurrencyName] = (file->currency(account.currencyId())).name();
      accountRow[ctPayee] = file->payee(loan.payee()).name();
      accountRow[ctLoanAmount] = (loan.loanAmount() * xr).toString();
      accountRow[ctInterestRate] = (loan.interestRate(QDate::currentDate()) / MyMoneyMoney(100, 1) * xr).toString();
      accountRow[ctNextInterestChange] = loan.nextInterestChange().toString(Qt::ISODate);
      accountRow[ctPeriodicPayment] = (loan.periodicPayment() * xr).toString();
      accountRow[ctFinalPayment] = (loan.finalPayment() * xr).toString();
      accountRow[ctFavorite] = account.value("PreferredAccount") == QLatin1String("Yes") ? i18nc("Is this a favorite account?", "Yes") : QString();

      MyMoneyMoney value = file->balance(account.id());
      value = value * xr;
      accountRow[ctCurrentBalance] = value.toString();
      m_rows += accountRow;
    }
    ++it_account;
  }
}

MyMoneyMoney ObjectInfoTable::investmentBalance(const MyMoneyAccount& acc)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyMoney value = file->balance(acc.id());
  QStringList accList = acc.accountList();

  QStringList::const_iterator it_a = accList.constBegin();
  for (; it_a != acc.accountList().constEnd(); ++it_a) {
    MyMoneyAccount stock = file->account(*it_a);
    try {
      MyMoneyMoney val;
      MyMoneyMoney balance = file->balance(stock.id());
      MyMoneySecurity security = file->security(stock.currencyId());
      const MyMoneyPrice &price = file->price(stock.currencyId(), security.tradingCurrency());
      val = balance * price.rate(security.tradingCurrency());
      // adjust value of security to the currency of the account
      MyMoneySecurity accountCurrency = file->currency(acc.currencyId());
      val = val * file->price(security.tradingCurrency(), accountCurrency.id()).rate(accountCurrency.id());
      val = val.convert(acc.fraction());
      value += val;
    } catch (const MyMoneyException &e) {
      qWarning("%s", qPrintable(QString("cannot convert stock balance of %1 to base currency: %2").arg(stock.name(), e.what())));
    }
  }
  return value;
}

}
