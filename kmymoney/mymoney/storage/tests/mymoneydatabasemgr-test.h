/***************************************************************************
                          mymoneydatabasemgrtest.h
                          -------------------
    copyright            : (C) 2008 by Fernando Vilas
    email                : fvilas@iname.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MYMONEYDATABASEMGRTEST_H
#define MYMONEYDATABASEMGRTEST_H

#include <QtCore/QObject>
#include <QTemporaryFile>
#include <QElapsedTimer>

#define KMM_MYMONEY_UNIT_TESTABLE friend class MyMoneyDatabaseMgrTest;

#include "../mymoneyobject.h"
#include "mymoneydatabasemgr.h"

class MyMoneyDatabaseMgrTest : public QObject
{
  Q_OBJECT

protected:
  MyMoneyDatabaseMgr *m;
  bool m_dbAttached;
  bool m_canOpen;
  bool m_haveEmptyDataBase;
  QUrl m_url;
  QTemporaryFile m_file;
  QTemporaryFile m_emptyFile;

private:
  QElapsedTimer testStepTimer;
  QElapsedTimer testCaseTimer;

public:
  MyMoneyDatabaseMgrTest();

private:
  void setupUrl(const QString& fname);
  void testEquality(const MyMoneyDatabaseMgr* t);
  void copyDatabaseFile(QFile& src, QFile& dest);

private slots:
  void init();
  void cleanup();
  void testEmptyConstructor();
  void testBadConnections();
  void testCreateDb();
  void testAttachDb();
  void testDisconnection();
  void testSetFunctions();
  void testIsStandardAccount();
  void testNewAccount();
  void testAccount();
  void testAddNewAccount();
  void testAddInstitution();
  void testInstitution();
  void testAccount2Institution();
  void testModifyAccount();
  void testModifyInstitution();
  void testReparentAccount();
  void testAddTransactions();
  void testTransactionCount();
  void testAddBudget();
  void testCopyBudget();
  void testModifyBudget();
  void testRemoveBudget();
  void testBalance();
  void testModifyTransaction();
  void testRemoveUnusedAccount();
  void testRemoveUsedAccount();
  void testRemoveInstitution();
  void testRemoveTransaction();
  void testTransactionList();
  void testAddPayee();
  void testSetAccountName();
  void testModifyPayee();
  void testPayeeName();
  void testRemovePayee();
  void testAddTag();
  void testModifyTag();
  void testTagName();
  void testRemoveTag();
  void testRemoveAccountFromTree();
  void testAssignment();
  void testDuplicate();
  void testAddSchedule();
  void testSchedule();
  void testModifySchedule();
  void testRemoveSchedule();
  void testSupportFunctions();
  void testScheduleList();
  void testAddCurrency();
  void testModifyCurrency();
  void testRemoveCurrency();
  void testCurrency();
  void testCurrencyList();
  void testAccountList();
  void testAddOnlineJob();
  void testModifyOnlineJob();
  void testRemoveOnlineJob();
  void testHighestNumberFromIdString();
};

#endif
