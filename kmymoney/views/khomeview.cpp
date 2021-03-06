/***************************************************************************
                          khomeview.cpp  -  description
                             -------------------
    begin                : Tue Jan 22 2002
    copyright            : (C) 2000-2002 by Michael Edwardes <mte@users.sourceforge.net>
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "khomeview.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QBuffer>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QWheelEvent>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#ifdef ENABLE_WEBENGINE
#include <QtWebEngineWidgets/QWebEngineView>
#else
#include <KDEWebKit/KWebView>
#endif

// ----------------------------------------------------------------------------
// KDE Includes

#include <KChartAbstractCoordinatePlane>
#include <KChartChart>
#include <KLocalizedString>
#include <KXmlGuiWindow>
#include <KActionCollection>
#include <KMessageBox>

// ----------------------------------------------------------------------------
// Project Includes
#include "mymoneyutils.h"
#include "kmymoneyutils.h"
#include "kwelcomepage.h"
#include "kmymoneyglobalsettings.h"
#include "mymoneyfile.h"
#include "mymoneyaccount.h"
#include "mymoneyprice.h"
#include "mymoneyforecast.h"
#include "kmymoney.h"
#include "kreportchartview.h"
#include "pivottable.h"
#include "pivotgrid.h"
#include "reportaccount.h"
#include "mymoneysplit.h"
#include "mymoneytransaction.h"
#include "icons.h"
#include "kmymoneywebpage.h"
#include "mymoneyschedule.h"
#include "mymoneyenums.h"

#define VIEW_LEDGER         "ledger"
#define VIEW_SCHEDULE       "schedule"
#define VIEW_WELCOME        "welcome"
#define VIEW_HOME           "home"
#define VIEW_REPORTS        "reports"

using namespace Icons;
using namespace eMyMoney;

bool accountNameLess(const MyMoneyAccount &acc1, const MyMoneyAccount &acc2)
{
  return acc1.name().localeAwareCompare(acc2.name()) < 0;
}

using namespace reports;

class KHomeView::Private
{
public:
  Private() :
      m_showAllSchedules(false),
      m_needReload(false),
      m_needLoad(true),
      m_netWorthGraphLastValidSize(400, 300) {
  }

  /**
   * daily balances of an account
   */
  typedef QMap<QDate, MyMoneyMoney> dailyBalances;

  #ifdef ENABLE_WEBENGINE
  QWebEngineView   *m_view;
  #else
  KWebView         *m_view;
  #endif

  QString           m_html;
  bool              m_showAllSchedules;
  bool              m_needReload;
  bool              m_needLoad;
  MyMoneyForecast   m_forecast;
  MyMoneyMoney      m_total;
  /**
    * Hold the last valid size of the net worth graph
    * for the times when the needed size can't be computed.
    */
  QSize           m_netWorthGraphLastValidSize;

  /**
    * daily forecast balance of accounts
    */
  QMap<QString, dailyBalances> m_accountList;
};

/**
 * @brief Converts a QPixmap to an data URI scheme
 *
 * According to RFC 2397
 *
 * @param pixmap Source to convert
 * @return full data URI
 */
QString QPixmapToDataUri(const QPixmap& pixmap)
{
  QImage image(pixmap.toImage());
  QByteArray byteArray;
  QBuffer buffer(&byteArray);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG"); // writes the image in PNG format inside the buffer
  return QLatin1String("data:image/png;base64,") + QString(byteArray.toBase64());
}

KHomeView::KHomeView(QWidget *parent) :
    KMyMoneyViewBase(parent),
    d(new Private)
{
}

KHomeView::~KHomeView()
{
  // if user wants to remember the font size, store it here
  if (KMyMoneyGlobalSettings::rememberZoomFactor()) {
    KMyMoneyGlobalSettings::setZoomFactor(d->m_view->zoomFactor());
    KMyMoneyGlobalSettings::self()->save();
  }

  delete d;
}

void KHomeView::init()
{
  d->m_needLoad = false;

  auto vbox = new QVBoxLayout(this);
  setLayout(vbox);
  vbox->setSpacing(6);
  vbox->setMargin(0);

#ifdef ENABLE_WEBENGINE
  d->m_view = new QWebEngineView(this);
#else
  d->m_view = new KWebView(this);
#endif
  d->m_view->setPage(new MyQWebEnginePage(d->m_view));

  vbox->addWidget(d->m_view);

  d->m_view->setHtml(KWelcomePage::welcomePage(), QUrl("file://"));
#ifdef ENABLE_WEBENGINE
  connect(d->m_view->page(), &QWebEnginePage::urlChanged,
          this, &KHomeView::slotOpenUrl);
#else
  d->m_view->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
  connect(d->m_view->page(), &KWebPage::linkClicked,
          this, &KHomeView::slotOpenUrl);
#endif
}

void KHomeView::wheelEvent(QWheelEvent* event)
{
  // Zoom text on Ctrl + Scroll
  if (event->modifiers() & Qt::CTRL) {
    qreal factor = d->m_view->zoomFactor();
    if (event->delta() > 0)
      factor += 0.1;
    else if (event->delta() < 0)
      factor -= 0.1;
    d->m_view->setZoomFactor(factor);
    event->accept();
    return;
  }
}

void KHomeView::slotLoadView()
{
  d->m_needReload = true;
  if (isVisible()) {
    loadView();
    d->m_needReload = false;
  }
}

void KHomeView::showEvent(QShowEvent* event)
{
  if (d->m_needLoad)
    init();

  emit aboutToShow(View::Home);

  if (d->m_needReload) {
    loadView();
    d->m_needReload = false;
  }

  QWidget::showEvent(event);
}

void KHomeView::slotPrintView()
{
  if (d->m_view) {
    m_currentPrinter = new QPrinter();
    QPrintDialog *dialog = new QPrintDialog(m_currentPrinter, this);
    dialog->setWindowTitle(QString());
    if (dialog->exec() != QDialog::Accepted) {
      delete m_currentPrinter;
      m_currentPrinter = nullptr;
      return;
    }
    #ifdef ENABLE_WEBENGINE
    d->m_view->page()->print(m_currentPrinter, [=] (bool) {delete m_currentPrinter; m_currentPrinter = nullptr;});
    #else
      d->m_view->print(m_currentPrinter);
    #endif
  }
}

void KHomeView::loadView()
{
  d->m_view->setZoomFactor(KMyMoneyGlobalSettings::zoomFactor());

  QList<MyMoneyAccount> list;
  MyMoneyFile::instance()->accountList(list);
  if (list.count() == 0) {
    d->m_view->setHtml(KWelcomePage::welcomePage(), QUrl("file://"));
  } else {
    //clear the forecast flag so it will be reloaded
    d->m_forecast.setForecastDone(false);

    const QString filename = QStandardPaths::locate(QStandardPaths::AppConfigLocation, "html/kmymoney.css");
    QString header = QString("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\">\n<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"%1\">\n").arg(QUrl::fromLocalFile(filename).url());

    header += KMyMoneyUtils::variableCSS();

    header += "</head><body id=\"summaryview\">\n";

    QString footer = "</body></html>\n";

    d->m_html.clear();
    d->m_html += header;

    d->m_html += QString("<div id=\"summarytitle\">%1</div>").arg(i18n("Your Financial Summary"));

    QStringList settings = KMyMoneyGlobalSettings::itemList();

    QStringList::ConstIterator it;

    for (it = settings.constBegin(); it != settings.constEnd(); ++it) {
      int option = (*it).toInt();
      if (option > 0) {
        switch (option) {
          case 1:         // payments
            showPayments();
            break;

          case 2:         // preferred accounts
            showAccounts(Preferred, i18n("Preferred Accounts"));
            break;

          case 3:         // payment accounts
            // Check if preferred accounts are shown separately
            if (settings.contains("2")) {
              showAccounts(static_cast<paymentTypeE>(Payment | Preferred),
                           i18n("Payment Accounts"));
            } else {
              showAccounts(Payment, i18n("Payment Accounts"));
            }
            break;
          case 4:         // favorite reports
            showFavoriteReports();
            break;
          case 5:         // forecast
            showForecast();
            break;
          case 6:         // net worth graph over all accounts
            showNetWorthGraph();
            break;
          case 8:         // assets and liabilities
            showAssetsLiabilities();
            break;
          case 9:         // budget
            showBudget();
            break;
          case 10:         // cash flow summary
            showCashFlowSummary();
            break;


        }
        d->m_html += "<div class=\"gap\">&nbsp;</div>\n";
      }
    }

    d->m_html += "<div id=\"returnlink\">";
    d->m_html += link(VIEW_WELCOME, QString()) + i18n("Show KMyMoney welcome page") + linkend();
    d->m_html += "</div>";
    d->m_html += "<div id=\"vieweffect\"></div>";
    d->m_html += footer;

    d->m_view->setHtml(d->m_html, QUrl("file://"));
  }
}

void KHomeView::showNetWorthGraph()
{
  d->m_html += QString("<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">%1</div>\n<div class=\"gap\">&nbsp;</div>\n").arg(i18n("Net Worth Forecast"));

  MyMoneyReport reportCfg = MyMoneyReport(
                              MyMoneyReport::eAssetLiability,
                              MyMoneyReport::eMonths,
                              TransactionFilter::Date::UserDefined, // overridden by the setDateFilter() call below
                              MyMoneyReport::eDetailTotal,
                              i18n("Net Worth Forecast"),
                              i18n("Generated Report"));

  reportCfg.setChartByDefault(true);
  reportCfg.setChartCHGridLines(false);
  reportCfg.setChartSVGridLines(false);
  reportCfg.setChartDataLabels(false);
  reportCfg.setChartType(MyMoneyReport::eChartLine);
  reportCfg.setIncludingSchedules(false);
  reportCfg.addAccountGroup(Account::Asset);
  reportCfg.addAccountGroup(Account::Liability);
  reportCfg.setColumnsAreDays(true);
  reportCfg.setConvertCurrency(true);
  reportCfg.setIncludingForecast(true);
  reportCfg.setDateFilter(QDate::currentDate(), QDate::currentDate().addDays(+ 90));
  reports::PivotTable table(reportCfg);

  reports::KReportChartView* chartWidget = new reports::KReportChartView(0);

  table.drawChart(*chartWidget);

  // Adjust the size
  QSize netWorthGraphSize = KHomeView::size();
  netWorthGraphSize -= QSize(80, 30);
  // consider the computed size valid only if it's smaller on both axes that the applications size
  if (netWorthGraphSize.width() < kmymoney->width() || netWorthGraphSize.height() < kmymoney->height()) {
    d->m_netWorthGraphLastValidSize = netWorthGraphSize;
  }
  chartWidget->resize(d->m_netWorthGraphLastValidSize);

  //save the chart to an image
  QString chart = QPixmapToDataUri(QPixmap::grabWidget(chartWidget->coordinatePlane()->parent()));

  d->m_html += QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >");
  d->m_html += QString("<tr>");
  d->m_html += QString("<td><center><img src=\"%1\" ALT=\"Networth\" width=\"100%\" ></center></td>").arg(chart);
  d->m_html += QString("</tr>");
  d->m_html += QString("</table></div></div>");

  //delete the widget since we no longer need it
  delete chartWidget;
}

void KHomeView::showPayments()
{
  MyMoneyFile* file = MyMoneyFile::instance();
  QList<MyMoneySchedule> overdues;
  QList<MyMoneySchedule> schedule;
  int i = 0;

  //if forecast has not been executed yet, do it.
  if (!d->m_forecast.isForecastDone())
    doForecast();

  schedule = file->scheduleList(QString(), Schedule::Type::Any,
                                Schedule::Occurrence::Any,
                                Schedule::PaymentType::Any,
                                QDate::currentDate(),
                                QDate::currentDate().addMonths(1), false);
  overdues = file->scheduleList(QString(), Schedule::Type::Any,
                                Schedule::Occurrence::Any,
                                Schedule::PaymentType::Any,
                                QDate(), QDate(), true);

  if (schedule.empty() && overdues.empty())
    return;

  // HACK
  // Remove the finished schedules
  QList<MyMoneySchedule>::Iterator d_it;
  //regular schedules
  d_it = schedule.begin();
  while (d_it != schedule.end()) {
    if ((*d_it).isFinished()) {
      d_it = schedule.erase(d_it);
      continue;
    }
    ++d_it;
  }
  //overdue schedules
  d_it = overdues.begin();
  while (d_it != overdues.end()) {
    if ((*d_it).isFinished()) {
      d_it = overdues.erase(d_it);
      continue;
    }
    ++d_it;
  }

  d->m_html += "<div class=\"shadow\"><div class=\"displayblock\">";
  d->m_html += QString("<div class=\"summaryheader\">%1</div>\n").arg(i18n("Payments"));

  if (!overdues.isEmpty()) {
    d->m_html += "<div class=\"gap\">&nbsp;</div>\n";

    qSort(overdues);
    QList<MyMoneySchedule>::Iterator it;
    QList<MyMoneySchedule>::Iterator it_f;

    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
    d->m_html += QString("<tr class=\"itemtitle warningtitle\" ><td colspan=\"5\">%1</td></tr>\n").arg(showColoredAmount(i18n("Overdue payments"), true));
    d->m_html += "<tr class=\"item warning\">";
    d->m_html += "<td class=\"left\" width=\"10%\">";
    d->m_html += i18n("Date");
    d->m_html += "</td>";
    d->m_html += "<td class=\"left\" width=\"40%\">";
    d->m_html += i18n("Schedule");
    d->m_html += "</td>";
    d->m_html += "<td class=\"left\" width=\"20%\">";
    d->m_html += i18n("Account");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"15%\">";
    d->m_html += i18n("Amount");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"15%\">";
    d->m_html += i18n("Balance after");
    d->m_html += "</td>";
    d->m_html += "</tr>";

    for (it = overdues.begin(); it != overdues.end(); ++it) {
      // determine number of overdue payments
      int cnt =
        (*it).transactionsRemainingUntil(QDate::currentDate().addDays(-1));

      d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
      showPaymentEntry(*it, cnt);
      d->m_html += "</tr>";
    }
    d->m_html += "</table>";
  }

  if (!schedule.isEmpty()) {
    qSort(schedule);

    // Extract todays payments if any
    QList<MyMoneySchedule> todays;
    QList<MyMoneySchedule>::Iterator t_it;
    for (t_it = schedule.begin(); t_it != schedule.end();) {
      if ((*t_it).adjustedNextDueDate() == QDate::currentDate()) {
        todays.append(*t_it);
        (*t_it).setNextDueDate((*t_it).nextPayment(QDate::currentDate()));

        // if adjustedNextDueDate is still currentDate then remove it from
        // scheduled payments
        if ((*t_it).adjustedNextDueDate() == QDate::currentDate()) {
          t_it = schedule.erase(t_it);
          continue;
        }
      }
      ++t_it;
    }

    if (todays.count() > 0) {
      d->m_html += "<div class=\"gap\">&nbsp;</div>\n";
      d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
      d->m_html += QString("<tr class=\"itemtitle\"><td class=\"left\" colspan=\"5\">%1</td></tr>\n").arg(i18n("Today's due payments"));
      d->m_html += "<tr class=\"item\">";
      d->m_html += "<td class=\"left\" width=\"10%\">";
      d->m_html += i18n("Date");
      d->m_html += "</td>";
      d->m_html += "<td class=\"left\" width=\"40%\">";
      d->m_html += i18n("Schedule");
      d->m_html += "</td>";
      d->m_html += "<td class=\"left\" width=\"20%\">";
      d->m_html += i18n("Account");
      d->m_html += "</td>";
      d->m_html += "<td class=\"right\" width=\"15%\">";
      d->m_html += i18n("Amount");
      d->m_html += "</td>";
      d->m_html += "<td class=\"right\" width=\"15%\">";
      d->m_html += i18n("Balance after");
      d->m_html += "</td>";
      d->m_html += "</tr>";

      for (t_it = todays.begin(); t_it != todays.end(); ++t_it) {
        d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
        showPaymentEntry(*t_it);
        d->m_html += "</tr>";
      }
      d->m_html += "</table>";
    }

    if (!schedule.isEmpty()) {
      d->m_html += "<div class=\"gap\">&nbsp;</div>\n";

      QList<MyMoneySchedule>::Iterator it;

      d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
      d->m_html += QString("<tr class=\"itemtitle\"><td class=\"left\" colspan=\"5\">%1</td></tr>\n").arg(i18n("Future payments"));
      d->m_html += "<tr class=\"item\">";
      d->m_html += "<td class=\"left\" width=\"10%\">";
      d->m_html += i18n("Date");
      d->m_html += "</td>";
      d->m_html += "<td class=\"left\" width=\"40%\">";
      d->m_html += i18n("Schedule");
      d->m_html += "</td>";
      d->m_html += "<td class=\"left\" width=\"20%\">";
      d->m_html += i18n("Account");
      d->m_html += "</td>";
      d->m_html += "<td class=\"right\" width=\"15%\">";
      d->m_html += i18n("Amount");
      d->m_html += "</td>";
      d->m_html += "<td class=\"right\" width=\"15%\">";
      d->m_html += i18n("Balance after");
      d->m_html += "</td>";
      d->m_html += "</tr>";

      // show all or the first 6 entries
      int cnt;
      cnt = (d->m_showAllSchedules) ? -1 : 6;
      bool needMoreLess = d->m_showAllSchedules;

      QDate lastDate = QDate::currentDate().addMonths(1);
      qSort(schedule);
      do {
        it = schedule.begin();
        if (it == schedule.end())
          break;

        // if the next due date is invalid (schedule is finished)
        // we remove it from the list
        QDate nextDate = (*it).nextDueDate();
        if (!nextDate.isValid()) {
          schedule.erase(it);
          continue;
        }

        if (nextDate > lastDate)
          break;

        if (cnt == 0) {
          needMoreLess = true;
          break;
        }

        // in case we've shown the current recurrence as overdue,
        // we don't show it here again, but keep the schedule
        // as it might show up later in the list again
        if (!(*it).isOverdue()) {
          if (cnt > 0)
            --cnt;

          d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
          showPaymentEntry(*it);
          d->m_html += "</tr>";

          // for single occurrence we have reported everything so we
          // better get out of here.
          if ((*it).occurrence() == Schedule::Occurrence::Once) {
            schedule.erase(it);
            continue;
          }
        }

        // if nextPayment returns an invalid date, setNextDueDate will
        // just skip it, resulting in a loop
        // we check the resulting date and erase the schedule if invalid
        if (!((*it).nextPayment((*it).nextDueDate())).isValid()) {
          schedule.erase(it);
          continue;
        }

        (*it).setNextDueDate((*it).nextPayment((*it).nextDueDate()));
        qSort(schedule);
      } while (1);

      if (needMoreLess) {
        d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
        d->m_html += "<td colspan=\"5\">";
        if (d->m_showAllSchedules) {
          d->m_html += link(VIEW_SCHEDULE,  QString("?mode=%1").arg("reduced")) + i18nc("Less...", "Show fewer schedules on the list") + linkend();
        } else {
          d->m_html += link(VIEW_SCHEDULE,  QString("?mode=%1").arg("full")) + i18nc("More...", "Show more schedules on the list") + linkend();
        }
        d->m_html += "</td>";
        d->m_html += "</tr>";
      }
      d->m_html += "</table>";
    }
  }
  d->m_html += "</div></div>";
}

void KHomeView::showPaymentEntry(const MyMoneySchedule& sched, int cnt)
{
  QString tmp;
  MyMoneyFile* file = MyMoneyFile::instance();

  try {
    MyMoneyAccount acc = sched.account();
    if (!acc.id().isEmpty()) {
      MyMoneyTransaction t = sched.transaction();
      // only show the entry, if it is still active
      if (!sched.isFinished()) {
        MyMoneySplit sp = t.splitByAccount(acc.id(), true);

        QString pathEnter = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::KeyEnter]).pixmap(QSize(16,16)));
        QString pathSkip = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::MediaSkipForward]).pixmap(QSize(16,16)));

        //show payment date
        tmp = QString("<td>") +
              QLocale().toString(sched.adjustedNextDueDate(), QLocale::ShortFormat) +
              "</td><td>";
        if (!pathEnter.isEmpty())
          tmp += link(VIEW_SCHEDULE, QString("?id=%1&amp;mode=enter").arg(sched.id()), i18n("Enter schedule")) + QString("<img src=\"%1\" border=\"0\"></a>").arg(pathEnter) + linkend();
        if (!pathSkip.isEmpty())
          tmp += "&nbsp;" + link(VIEW_SCHEDULE, QString("?id=%1&amp;mode=skip").arg(sched.id()), i18n("Skip schedule")) + QString("<img src=\"%1\" border=\"0\"></a>").arg(pathSkip) + linkend();

        tmp += QString("&nbsp;");
        tmp += link(VIEW_SCHEDULE, QString("?id=%1&amp;mode=edit").arg(sched.id()), i18n("Edit schedule")) + sched.name() + linkend();

        //show quantity of payments overdue if any
        if (cnt > 1)
          tmp += i18np(" (%1 payment)", " (%1 payments)", cnt);

        //show account of the main split
        tmp += "</td><td>";
        tmp += QString(file->account(acc.id()).name());

        //show amount of the schedule
        tmp += "</td><td align=\"right\">";

        const MyMoneySecurity& currency = MyMoneyFile::instance()->currency(acc.currencyId());
        MyMoneyMoney payment = MyMoneyMoney(sp.value(t.commodity(), acc.currencyId()) * cnt);
        QString amount = MyMoneyUtils::formatMoney(payment, acc, currency);
        amount.replace(QChar(' '), "&nbsp;");
        tmp += showColoredAmount(amount, payment.isNegative());
        tmp += "</td>";
        //show balance after payments
        tmp += "<td align=\"right\">";
        QDate paymentDate = QDate(sched.adjustedNextDueDate());
        MyMoneyMoney balanceAfter = forecastPaymentBalance(acc, payment, paymentDate);
        QString balance = MyMoneyUtils::formatMoney(balanceAfter, acc, currency);
        balance.replace(QChar(' '), "&nbsp;");
        tmp += showColoredAmount(balance, balanceAfter.isNegative());
        tmp += "</td>";

        // qDebug("paymentEntry = '%s'", tmp.toLatin1());
        d->m_html += tmp;
      }
    }
  } catch (const MyMoneyException &e) {
    qDebug("Unable to display schedule entry: %s", qPrintable(e.what()));
  }
}

void KHomeView::showAccounts(KHomeView::paymentTypeE type, const QString& header)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  int prec = MyMoneyMoney::denomToPrec(file->baseCurrency().smallestAccountFraction());
  QList<MyMoneyAccount> accounts;

  bool showClosedAccounts = kmymoney->isActionToggled(Action::ViewShowAll);

  // get list of all accounts
  file->accountList(accounts);
  for (QList<MyMoneyAccount>::Iterator it = accounts.begin(); it != accounts.end();) {
    bool removeAccount = false;
    if (!(*it).isClosed() || showClosedAccounts) {
      switch ((*it).accountType()) {
        case Account::Expense:
        case Account::Income:
          // never show a category account
          // Note: This might be different in a future version when
          //       the homepage also shows category based information
          removeAccount = true;
          break;

          // Asset and Liability accounts are only shown if they
          // have the preferred flag set
        case Account::Asset:
        case Account::Liability:
        case Account::Investment:
          // if preferred accounts are requested, then keep in list
          if ((*it).value("PreferredAccount") != "Yes"
              || (type & Preferred) == 0) {
            removeAccount = true;
          }
          break;

          // Check payment accounts. If payment and preferred is selected,
          // then always show them. If only payment is selected, then
          // show only if preferred flag is not set.
        case Account::Checkings:
        case Account::Savings:
        case Account::Cash:
        case Account::CreditCard:
          switch (type & (Payment | Preferred)) {
            case Payment:
              if ((*it).value("PreferredAccount") == "Yes")
                removeAccount = true;
              break;

            case Preferred:
              if ((*it).value("PreferredAccount") != "Yes")
                removeAccount = true;
              break;

            case Payment | Preferred:
              break;

            default:
              removeAccount = true;
              break;
          }
          break;

          // filter all accounts that are not used on homepage views
        default:
          removeAccount = true;
          break;
      }

    } else if ((*it).isClosed() || (*it).isInvest()) {
      // don't show if closed or a stock account
      removeAccount = true;
    }

    if (removeAccount)
      it = accounts.erase(it);
    else
      ++it;
  }

  if (!accounts.isEmpty()) {
    // sort the accounts by name
    qStableSort(accounts.begin(), accounts.end(), accountNameLess);
    QString tmp;
    int i = 0;
    tmp = "<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">" + header + "</div>\n<div class=\"gap\">&nbsp;</div>\n";
    d->m_html += tmp;
    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
    d->m_html += "<tr class=\"item\">";

    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) {
      QString pathStatusHeader = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::Download]).pixmap(QSize(16,16)));
      d->m_html += QString("<td class=\"center\"><img src=\"%1\" border=\"0\"></td>").arg(pathStatusHeader);
    }

    d->m_html += "<td class=\"left\" width=\"35%\">";
    d->m_html += i18n("Account");
    d->m_html += "</td>";

    if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions())
      d->m_html += QString("<td class=\"center\">!M</td>");

    if (KMyMoneyGlobalSettings::showCountOfClearedTransactions())
      d->m_html += QString("<td class=\"center\">C</td>");

    if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
      d->m_html += QString("<td class=\"center\">!R</td>");

    d->m_html += "<td width=\"25%\" class=\"right\">";
    d->m_html += i18n("Current Balance");
    d->m_html += "</td>";

    //only show limit info if user chose to do so
    if (KMyMoneyGlobalSettings::showLimitInfo()) {
      d->m_html += "<td width=\"40%\" class=\"right\">";
      d->m_html += i18n("To Minimum Balance / Maximum Credit");
      d->m_html += "</td>";
    }
    d->m_html += "</tr>";

    d->m_total = 0;
    QList<MyMoneyAccount>::const_iterator it_m;
    for (it_m = accounts.constBegin(); it_m != accounts.constEnd(); ++it_m) {
      d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
      showAccountEntry(*it_m);
      d->m_html += "</tr>";
    }
    d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
    QString amount = d->m_total.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) d->m_html += "<td></td>";
    d->m_html += QString("<td class=\"right\"><b>%1</b></td>").arg(i18n("Total"));
    if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions()) d->m_html += "<td></td>";
    if (KMyMoneyGlobalSettings::showCountOfClearedTransactions()) d->m_html += "<td></td>";
    if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions()) d->m_html += "<td></td>";
    d->m_html += QString("<td class=\"right\"><b>%1</b></td></tr>").arg(showColoredAmount(amount, d->m_total.isNegative()));
    d->m_html += "</table></div></div>";
  }
}

void KHomeView::showAccountEntry(const MyMoneyAccount& acc)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneySecurity currency = file->currency(acc.currencyId());
  MyMoneyMoney value;

  bool showLimit = KMyMoneyGlobalSettings::showLimitInfo();

  if (acc.accountType() == Account::Investment) {
    //investment accounts show the balances of all its subaccounts
    value = investmentBalance(acc);

    //investment accounts have no minimum balance
    showAccountEntry(acc, value, MyMoneyMoney(), showLimit);
  } else {
    //get balance for normal accounts
    value = file->balance(acc.id(), QDate::currentDate());
    if (acc.currencyId() != file->baseCurrency().id()) {
      ReportAccount repAcc = ReportAccount(acc.id());
      MyMoneyMoney curPrice = repAcc.baseCurrencyPrice(QDate::currentDate());
      MyMoneyMoney baseValue = value * curPrice;
      baseValue = baseValue.convert(file->baseCurrency().smallestAccountFraction());
      d->m_total += baseValue;
    } else {
      d->m_total += value;
    }
    //if credit card or checkings account, show maximum credit
    if (acc.accountType() == Account::CreditCard ||
        acc.accountType() == Account::Checkings) {
      QString maximumCredit = acc.value("maxCreditAbsolute");
      if (maximumCredit.isEmpty()) {
        maximumCredit = acc.value("minBalanceAbsolute");
      }
      MyMoneyMoney maxCredit = MyMoneyMoney(maximumCredit);
      showAccountEntry(acc, value, value - maxCredit, showLimit);
    } else {
      //otherwise use minimum balance
      QString minimumBalance = acc.value("minBalanceAbsolute");
      MyMoneyMoney minBalance = MyMoneyMoney(minimumBalance);
      showAccountEntry(acc, value, value - minBalance, showLimit);
    }
  }
}

void KHomeView::showAccountEntry(const MyMoneyAccount& acc, const MyMoneyMoney& value, const MyMoneyMoney& valueToMinBal, const bool showMinBal)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  QString tmp;
  MyMoneySecurity currency = file->currency(acc.currencyId());
  QString amount;
  QString amountToMinBal;

  //format amounts
  amount = MyMoneyUtils::formatMoney(value, acc, currency);
  amount.replace(QChar(' '), "&nbsp;");
  if (showMinBal) {
    amountToMinBal = MyMoneyUtils::formatMoney(valueToMinBal, acc, currency);
    amountToMinBal.replace(QChar(' '), "&nbsp;");
  }

  QString cellStatus, cellCounts, pathOK, pathTODO, pathNotOK;

  if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) {
    //show account's online-status
    pathOK = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::DialogOKApply]).pixmap(QSize(16,16)));
    pathTODO = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::MailReceive]).pixmap(QSize(16,16)));
    pathNotOK = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::DialogCancel]).pixmap(QSize(16,16)));

    if (acc.value("lastImportedTransactionDate").isEmpty() || acc.value("lastStatementBalance").isEmpty())
      cellStatus = '-';
    else if (file->hasMatchingOnlineBalance(acc)) {
      if (file->hasNewerTransaction(acc.id(), QDate::fromString(acc.value("lastImportedTransactionDate"), Qt::ISODate)))
        cellStatus = QString("<img src=\"%1\" border=\"0\">").arg(pathTODO);
      else
        cellStatus = QString("<img src=\"%1\" border=\"0\">").arg(pathOK);
    } else
      cellStatus = QString("<img src=\"%1\" border=\"0\">").arg(pathNotOK);

    tmp = QString("<td class=\"center\">%1</td>").arg(cellStatus);
  }

  tmp += QString("<td>") +
         link(VIEW_LEDGER, QString("?id=%1").arg(acc.id())) + acc.name() + linkend() + "</td>";

  int countNotMarked = 0, countCleared = 0, countNotReconciled = 0;
  QString countStr;

  if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions() || KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
    countNotMarked = file->countTransactionsWithSpecificReconciliationState(acc.id(), TransactionFilter::State::NotReconciled);

  if (KMyMoneyGlobalSettings::showCountOfClearedTransactions() || KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
    countCleared = file->countTransactionsWithSpecificReconciliationState(acc.id(), TransactionFilter::State::Cleared);

  if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
    countNotReconciled = countNotMarked + countCleared;

  if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions()) {
    if (countNotMarked)
      countStr = QString("%1").arg(countNotMarked);
    else
      countStr = '-';
    tmp += QString("<td class=\"center\">%1</td>").arg(countStr);
  }

  if (KMyMoneyGlobalSettings::showCountOfClearedTransactions()) {
    if (countCleared)
      countStr = QString("%1").arg(countCleared);
    else
      countStr = '-';
    tmp += QString("<td class=\"center\">%1</td>").arg(countStr);
  }

  if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions()) {
    if (countNotReconciled)
      countStr = QString("%1").arg(countNotReconciled);
    else
      countStr = '-';
    tmp += QString("<td class=\"center\">%1</td>").arg(countStr);
  }

  //show account balance
  tmp += QString("<td class=\"right\">%1</td>").arg(showColoredAmount(amount, value.isNegative()));

  //show minimum balance column if requested
  if (showMinBal) {
    //if it is an investment, show minimum balance empty
    if (acc.accountType() == Account::Investment) {
      tmp += QString("<td class=\"right\">&nbsp;</td>");
    } else {
      //show minimum balance entry
      tmp += QString("<td class=\"right\">%1</td>").arg(showColoredAmount(amountToMinBal, valueToMinBal.isNegative()));
    }
  }
  // qDebug("accountEntry = '%s'", tmp.toLatin1());
  d->m_html += tmp;
}

MyMoneyMoney KHomeView::investmentBalance(const MyMoneyAccount& acc)
{
  auto file = MyMoneyFile::instance();
  auto value = file->balance(acc.id(), QDate::currentDate());
  foreach (const auto accountID, acc.accountList()) {
    auto stock = file->account(accountID);
    if (!stock.isClosed()) {
      try {
        MyMoneyMoney val;
        MyMoneyMoney balance = file->balance(stock.id(), QDate::currentDate());
        MyMoneySecurity security = file->security(stock.currencyId());
        const MyMoneyPrice &price = file->price(stock.currencyId(), security.tradingCurrency());
        val = (balance * price.rate(security.tradingCurrency())).convertPrecision(security.pricePrecision());
        // adjust value of security to the currency of the account
        MyMoneySecurity accountCurrency = file->currency(acc.currencyId());
        val = val * file->price(security.tradingCurrency(), accountCurrency.id()).rate(accountCurrency.id());
        val = val.convert(acc.fraction());
        value += val;
      } catch (const MyMoneyException &e) {
        qWarning("%s", qPrintable(QString("cannot convert stock balance of %1 to base currency: %2").arg(stock.name(), e.what())));
      }
    }
  }
  return value;
}

void KHomeView::showFavoriteReports()
{
  QList<MyMoneyReport> reports = MyMoneyFile::instance()->reportList();

  if (!reports.isEmpty()) {
    bool firstTime = 1;
    int row = 0;
    QList<MyMoneyReport>::const_iterator it_report = reports.constBegin();
    while (it_report != reports.constEnd()) {
      if ((*it_report).isFavorite()) {
        if (firstTime) {
          d->m_html += QString("<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">%1</div>\n<div class=\"gap\">&nbsp;</div>\n").arg(i18n("Favorite Reports"));
          d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
          d->m_html += "<tr class=\"item\"><td class=\"left\" width=\"40%\">";
          d->m_html += i18n("Report");
          d->m_html += "</td><td width=\"60%\" class=\"left\">";
          d->m_html += i18n("Comment");
          d->m_html += "</td></tr>";
          firstTime = false;
        }

        d->m_html += QString("<tr class=\"row-%1\"><td>%2%3%4</td><td align=\"left\">%5</td></tr>")
                     .arg(row++ & 0x01 ? "even" : "odd")
                     .arg(link(VIEW_REPORTS, QString("?id=%1").arg((*it_report).id())))
                     .arg((*it_report).name())
                     .arg(linkend())
                     .arg((*it_report).comment());
      }

      ++it_report;
    }
    if (!firstTime)
      d->m_html += "</table></div></div>";
  }
}

void KHomeView::showForecast()
{
  MyMoneyFile* file = MyMoneyFile::instance();
  QList<MyMoneyAccount> accList;

  //if forecast has not been executed yet, do it.
  if (!d->m_forecast.isForecastDone())
    doForecast();

  accList = d->m_forecast.accountList();

  if (accList.count() > 0) {
    // sort the accounts by name
    qStableSort(accList.begin(), accList.end(), accountNameLess);
    int i = 0;

    int colspan = 1;
    //get begin day
    int beginDay = QDate::currentDate().daysTo(d->m_forecast.beginForecastDate());
    //if begin day is today skip to next cycle
    if (beginDay == 0)
      beginDay = d->m_forecast.accountsCycle();

    // Now output header
    d->m_html += QString("<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">%1</div>\n<div class=\"gap\">&nbsp;</div>\n").arg(i18n("%1 Day Forecast", d->m_forecast.forecastDays()));
    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
    d->m_html += "<tr class=\"item\"><td class=\"left\" width=\"40%\">";
    d->m_html += i18n("Account");
    d->m_html += "</td>";
    int colWidth = 55 / (d->m_forecast.forecastDays() / d->m_forecast.accountsCycle());
    for (i = 0; (i*d->m_forecast.accountsCycle() + beginDay) <= d->m_forecast.forecastDays(); ++i) {
      d->m_html += QString("<td width=\"%1%\" class=\"right\">").arg(colWidth);

      d->m_html += i18ncp("Forecast days", "%1 day", "%1 days", i * d->m_forecast.accountsCycle() + beginDay);
      d->m_html += "</td>";
      colspan++;
    }
    d->m_html += "</tr>";

    // Now output entries
    i = 0;

    QList<MyMoneyAccount>::ConstIterator it_account;
    for (it_account = accList.constBegin(); it_account != accList.constEnd(); ++it_account) {
      //MyMoneyAccount acc = (*it_n);

      d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
      d->m_html += QString("<td width=\"40%\">") +
                   link(VIEW_LEDGER, QString("?id=%1").arg((*it_account).id())) + (*it_account).name() + linkend() + "</td>";

      int dropZero = -1; //account dropped below zero
      int dropMinimum = -1; //account dropped below minimum balance
      QString minimumBalance = (*it_account).value("minimumBalance");
      MyMoneyMoney minBalance = MyMoneyMoney(minimumBalance);
      MyMoneySecurity currency;
      MyMoneyMoney forecastBalance;

      //change account to deep currency if account is an investment
      if ((*it_account).isInvest()) {
        MyMoneySecurity underSecurity = file->security((*it_account).currencyId());
        currency = file->security(underSecurity.tradingCurrency());
      } else {
        currency = file->security((*it_account).currencyId());
      }

      for (int f = beginDay; f <= d->m_forecast.forecastDays(); f += d->m_forecast.accountsCycle()) {
        forecastBalance = d->m_forecast.forecastBalance(*it_account, QDate::currentDate().addDays(f));
        QString amount;
        amount = MyMoneyUtils::formatMoney(forecastBalance, *it_account, currency);
        amount.replace(QChar(' '), "&nbsp;");
        d->m_html += QString("<td width=\"%1%\" align=\"right\">").arg(colWidth);
        d->m_html += QString("%1</td>").arg(showColoredAmount(amount, forecastBalance.isNegative()));
      }

      d->m_html += "</tr>";

      //Check if the account is going to be below zero or below the minimal balance in the forecast period

      //Check if the account is going to be below minimal balance
      dropMinimum = d->m_forecast.daysToMinimumBalance(*it_account);

      //Check if the account is going to be below zero in the future
      dropZero = d->m_forecast.daysToZeroBalance(*it_account);


      // spit out possible warnings
      QString msg;

      // if a minimum balance has been specified, an appropriate warning will
      // only be shown, if the drop below 0 is on a different day or not present

      if (dropMinimum != -1
          && !minBalance.isZero()
          && (dropMinimum < dropZero
              || dropZero == -1)) {
        switch (dropMinimum) {
          case -1:
            break;
          case 0:
            msg = i18n("The balance of %1 is below the minimum balance %2 today.", (*it_account).name(), MyMoneyUtils::formatMoney(minBalance, *it_account, currency));
            msg = showColoredAmount(msg, true);
            break;
          default:
            msg = i18np("The balance of %2 will drop below the minimum balance %3 in %1 day.",
                        "The balance of %2 will drop below the minimum balance %3 in %1 days.",
                        dropMinimum - 1, (*it_account).name(), MyMoneyUtils::formatMoney(minBalance, *it_account, currency));
            msg = showColoredAmount(msg, true);
            break;
        }

        if (!msg.isEmpty()) {
          d->m_html += QString("<tr class=\"warning\" style=\"font-weight: normal;\" ><td colspan=%2 align=\"center\" >%1</td></tr>").arg(msg).arg(colspan);
        }
      }
      // a drop below zero is always shown
      msg.clear();
      switch (dropZero) {
        case -1:
          break;
        case 0:
          if ((*it_account).accountGroup() == Account::Asset) {
            msg = i18n("The balance of %1 is below %2 today.", (*it_account).name(), MyMoneyUtils::formatMoney(MyMoneyMoney(), *it_account, currency));
            msg = showColoredAmount(msg, true);
            break;
          }
          if ((*it_account).accountGroup() == Account::Liability) {
            msg = i18n("The balance of %1 is above %2 today.", (*it_account).name(), MyMoneyUtils::formatMoney(MyMoneyMoney(), *it_account, currency));
            break;
          }
          break;
        default:
          if ((*it_account).accountGroup() == Account::Asset) {
            msg = i18np("The balance of %2 will drop below %3 in %1 day.",
                        "The balance of %2 will drop below %3 in %1 days.",
                        dropZero, (*it_account).name(), MyMoneyUtils::formatMoney(MyMoneyMoney(), *it_account, currency));
            msg = showColoredAmount(msg, true);
            break;
          }
          if ((*it_account).accountGroup() == Account::Liability) {
            msg = i18np("The balance of %2 will raise above %3 in %1 day.",
                        "The balance of %2 will raise above %3 in %1 days.",
                        dropZero, (*it_account).name(), MyMoneyUtils::formatMoney(MyMoneyMoney(), *it_account, currency));
            break;
          }
      }
      if (!msg.isEmpty()) {
        d->m_html += QString("<tr class=\"warning\"><td colspan=%2 align=\"center\" ><b>%1</b></td></tr>").arg(msg).arg(colspan);
      }
    }
    d->m_html += "</table></div></div>";

  }
}

QString KHomeView::link(const QString& view, const QString& query, const QString& _title) const
{
  QString titlePart;
  QString title(_title);
  if (!title.isEmpty())
    titlePart = QString(" title=\"%1\"").arg(title.replace(QLatin1Char(' '), "&nbsp;"));

  return QString("<a href=\"/%1%2\"%3>").arg(view, query, titlePart);
}

QString KHomeView::linkend() const
{
  return QStringLiteral("</a>");
}

void KHomeView::slotOpenUrl(const QUrl &url)
{
  QString protocol = url.scheme();
  QString view = url.fileName();
  if (view.isEmpty())
    return;
  QUrlQuery query(url);
  QString id = query.queryItemValue("id");
  QString mode = query.queryItemValue("mode");

  if (protocol == QLatin1String("http")) {
    QDesktopServices::openUrl(url);
  } else if (protocol == QLatin1String("mailto")) {
    QDesktopServices::openUrl(url);
  } else {
    KXmlGuiWindow* mw = KMyMoneyUtils::mainWindow();
    Q_CHECK_PTR(mw);
    if (view == VIEW_LEDGER) {
      emit ledgerSelected(id, QString());

    } else if (view == VIEW_SCHEDULE) {
      if (mode == QLatin1String("enter")) {
        emit scheduleSelected(id);
        QTimer::singleShot(0, mw->actionCollection()->action(kmymoney->s_Actions[Action::ScheduleEnter]), SLOT(trigger()));
      } else if (mode == QLatin1String("edit")) {
        emit scheduleSelected(id);
        QTimer::singleShot(0, mw->actionCollection()->action(kmymoney->s_Actions[Action::ScheduleEdit]), SLOT(trigger()));
      } else if (mode == QLatin1String("skip")) {
        emit scheduleSelected(id);
        QTimer::singleShot(0, mw->actionCollection()->action(kmymoney->s_Actions[Action::ScheduleSkip]), SLOT(trigger()));
      } else if (mode == QLatin1String("full")) {
        d->m_showAllSchedules = true;
        loadView();

      } else if (mode == QLatin1String("reduced")) {
        d->m_showAllSchedules = false;
        loadView();
      }

    } else if (view == VIEW_REPORTS) {
      emit reportSelected(id);

    } else if (view == VIEW_WELCOME) {
      if (mode == QLatin1String("whatsnew"))
        d->m_view->setHtml(KWelcomePage::whatsNewPage(), QUrl("file://"));
      else
        d->m_view->setHtml(KWelcomePage::welcomePage(), QUrl("file://"));

    } else if (view == QLatin1String("action")) {
      QTimer::singleShot(0, mw->actionCollection()->action(id), SLOT(trigger()));
    } else if (view == VIEW_HOME) {
      QList<MyMoneyAccount> list;
      MyMoneyFile::instance()->accountList(list);
      if (list.count() == 0) {
        KMessageBox::information(this, i18n("Before KMyMoney can give you detailed information about your financial status, you need to create at least one account. Until then, KMyMoney shows the welcome page instead."));
      }
      loadView();

    } else {
      qDebug("Unknown view '%s' in KHomeView::slotOpenURL()", qPrintable(view));
    }
  }
}

void KHomeView::showAssetsLiabilities()
{
  QList<MyMoneyAccount> accounts;
  QList<MyMoneyAccount>::ConstIterator it;
  QList<MyMoneyAccount> assets;
  QList<MyMoneyAccount> liabilities;
  MyMoneyMoney netAssets;
  MyMoneyMoney netLiabilities;
  QString fontStart, fontEnd;

  MyMoneyFile* file = MyMoneyFile::instance();
  int prec = MyMoneyMoney::denomToPrec(file->baseCurrency().smallestAccountFraction());
  int i = 0;

  // get list of all accounts
  file->accountList(accounts);

  for (it = accounts.constBegin(); it != accounts.constEnd();) {
    if (!(*it).isClosed()) {
      switch ((*it).accountType()) {
          // group all assets into one list but make sure that investment accounts always show up
        case Account::Investment:
          assets << *it;
          break;

        case Account::Checkings:
        case Account::Savings:
        case Account::Cash:
        case Account::Asset:
        case Account::AssetLoan:
          // list account if it's the last in the hierarchy or has transactions in it
          if ((*it).accountList().isEmpty() || (file->transactionCount((*it).id()) > 0)) {
            assets << *it;
          }
          break;

          // group the liabilities into the other
        case Account::CreditCard:
        case Account::Liability:
        case Account::Loan:
          // list account if it's the last in the hierarchy or has transactions in it
          if ((*it).accountList().isEmpty() || (file->transactionCount((*it).id()) > 0)) {
            liabilities << *it;
          }
          break;

        default:
          break;
      }
    }
    ++it;
  }

  //only do it if we have assets or liabilities account
  if (assets.count() > 0 || liabilities.count() > 0) {
    // sort the accounts by name
    qStableSort(assets.begin(), assets.end(), accountNameLess);
    qStableSort(liabilities.begin(), liabilities.end(), accountNameLess);
    QString statusHeader;
    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) {
      QString pathStatusHeader;
      pathStatusHeader = QPixmapToDataUri(QIcon::fromTheme(g_Icons[Icon::ViewOutbox]).pixmap(QSize(16,16)));
      statusHeader = QString("<img src=\"%1\" border=\"0\">").arg(pathStatusHeader);
    }

    //print header
    d->m_html += "<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">" + i18n("Assets and Liabilities Summary") + "</div>\n<div class=\"gap\">&nbsp;</div>\n";
    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";

    //column titles

    d->m_html += "<tr class=\"item\">";

    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) {
      d->m_html += "<td class=\"setcolor\">";
      d->m_html += statusHeader;
      d->m_html += "</td>";
    }

    d->m_html += "<td class=\"left\" width=\"30%\">";
    d->m_html += i18n("Asset Accounts");
    d->m_html += "</td>";

    if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions())
      d->m_html += "<td class=\"setcolor\">!M</td>";

    if (KMyMoneyGlobalSettings::showCountOfClearedTransactions())
      d->m_html += "<td class=\"setcolor\">C</td>";

    if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
      d->m_html += "<td class=\"setcolor\">!R</td>";

    d->m_html += "<td width=\"15%\" class=\"right\">";
    d->m_html += i18n("Current Balance");
    d->m_html += "</td>";

    //intermediate row to separate both columns
    d->m_html += "<td width=\"10%\" class=\"setcolor\"></td>";

    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) {
      d->m_html += "<td class=\"setcolor\">";
      d->m_html += statusHeader;
      d->m_html += "</td>";
    }

    d->m_html += "<td class=\"left\" width=\"30%\">";
    d->m_html += i18n("Liability Accounts");
    d->m_html += "</td>";

    if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions())
      d->m_html += "<td class=\"setcolor\">!M</td>";

    if (KMyMoneyGlobalSettings::showCountOfClearedTransactions())
      d->m_html += "<td class=\"setcolor\">C</td>";

    if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions())
      d->m_html += "<td class=\"setcolor\">!R</td>";

    d->m_html += "<td width=\"15%\" class=\"right\">";
    d->m_html += i18n("Current Balance");
    d->m_html += "</td></tr>";

    QString placeHolder_Status, placeHolder_Counts;
    if (KMyMoneyGlobalSettings::showBalanceStatusOfOnlineAccounts()) placeHolder_Status = "<td></td>";
    if (KMyMoneyGlobalSettings::showCountOfUnmarkedTransactions()) placeHolder_Counts = "<td></td>";
    if (KMyMoneyGlobalSettings::showCountOfClearedTransactions()) placeHolder_Counts += "<td></td>";
    if (KMyMoneyGlobalSettings::showCountOfNotReconciledTransactions()) placeHolder_Counts += "<td></td>";

    //get asset and liability accounts
    QList<MyMoneyAccount>::const_iterator asset_it = assets.constBegin();
    QList<MyMoneyAccount>::const_iterator liabilities_it = liabilities.constBegin();
    for (; asset_it != assets.constEnd() || liabilities_it != liabilities.constEnd();) {
      d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");
      //write an asset account if we still have any
      if (asset_it != assets.constEnd()) {
        MyMoneyMoney value;
        //investment accounts consolidate the balance of its subaccounts
        if ((*asset_it).accountType() == Account::Investment) {
          value = investmentBalance(*asset_it);
        } else {
          value = MyMoneyFile::instance()->balance((*asset_it).id(), QDate::currentDate());
        }

        //calculate balance for foreign currency accounts
        if ((*asset_it).currencyId() != file->baseCurrency().id()) {
          ReportAccount repAcc = ReportAccount((*asset_it).id());
          MyMoneyMoney curPrice = repAcc.baseCurrencyPrice(QDate::currentDate());
          MyMoneyMoney baseValue = value * curPrice;
          baseValue = baseValue.convert(10000);
          netAssets += baseValue;
        } else {
          netAssets += value;
        }
        //show the account without minimum balance
        showAccountEntry(*asset_it, value, MyMoneyMoney(), false);
        ++asset_it;
      } else {
        //write a white space if we don't
        d->m_html += QString("%1<td></td>%2<td></td>").arg(placeHolder_Status).arg(placeHolder_Counts);
      }

      //leave the intermediate column empty
      d->m_html += "<td class=\"setcolor\"></td>";

      //write a liability account
      if (liabilities_it != liabilities.constEnd()) {
        MyMoneyMoney value;
        value = MyMoneyFile::instance()->balance((*liabilities_it).id(), QDate::currentDate());
        //calculate balance if foreign currency
        if ((*liabilities_it).currencyId() != file->baseCurrency().id()) {
          ReportAccount repAcc = ReportAccount((*liabilities_it).id());
          MyMoneyMoney curPrice = repAcc.baseCurrencyPrice(QDate::currentDate());
          MyMoneyMoney baseValue = value * curPrice;
          baseValue = baseValue.convert(10000);
          netLiabilities += baseValue;
        } else {
          netLiabilities += value;
        }
        //show the account without minimum balance
        showAccountEntry(*liabilities_it, value, MyMoneyMoney(), false);
        ++liabilities_it;
      } else {
        //leave the space empty if we run out of liabilities
        d->m_html += QString("%1<td></td>%2<td></td>").arg(placeHolder_Status).arg(placeHolder_Counts);
      }
      d->m_html += "</tr>";
    }

    //calculate net worth
    MyMoneyMoney netWorth = netAssets + netLiabilities;

    //format assets, liabilities and net worth
    QString amountAssets = netAssets.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    QString amountLiabilities = netLiabilities.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    QString amountNetWorth = netWorth.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    amountAssets.replace(QChar(' '), "&nbsp;");
    amountLiabilities.replace(QChar(' '), "&nbsp;");
    amountNetWorth.replace(QChar(' '), "&nbsp;");

    d->m_html += QString("<tr class=\"row-%1\" style=\"font-weight:bold;\">").arg(i++ & 0x01 ? "even" : "odd");

    //print total for assets
    d->m_html += QString("%1<td class=\"left\">%2</td>%3<td align=\"right\">%4</td>").arg(placeHolder_Status).arg(i18n("Total Assets")).arg(placeHolder_Counts).arg(showColoredAmount(amountAssets, netAssets.isNegative()));

    //leave the intermediate column empty
    d->m_html += "<td class=\"setcolor\"></td>";

    //print total liabilities
    d->m_html += QString("%1<td class=\"left\">%2</td>%3<td align=\"right\">%4</td>").arg(placeHolder_Status).arg(i18n("Total Liabilities")).arg(placeHolder_Counts).arg(showColoredAmount(amountLiabilities, netLiabilities.isNegative()));
    d->m_html += "</tr>";

    //print net worth
    d->m_html += QString("<tr class=\"row-%1\" style=\"font-weight:bold;\">").arg(i++ & 0x01 ? "even" : "odd");

    d->m_html += QString("%1<td></td><td></td>%2<td class=\"setcolor\"></td>").arg(placeHolder_Status).arg(placeHolder_Counts);
    d->m_html += QString("%1<td class=\"left\">%2</td>%3<td align=\"right\">%4</td>").arg(placeHolder_Status).arg(i18n("Net Worth")).arg(placeHolder_Counts).arg(showColoredAmount(amountNetWorth, netWorth.isNegative()));

    d->m_html += "</tr>";
    d->m_html += "</table>";
    d->m_html += "</div></div>";
  }
}

void KHomeView::showBudget()
{
  MyMoneyFile* file = MyMoneyFile::instance();

  if (file->countBudgets()) {
    int prec = MyMoneyMoney::denomToPrec(file->baseCurrency().smallestAccountFraction());
    bool isOverrun = false;
    int i = 0;

    //config report just like "Monthly Budgeted vs Actual
    MyMoneyReport reportCfg = MyMoneyReport(
                                MyMoneyReport::eBudgetActual,
                                MyMoneyReport::eMonths,
                                TransactionFilter::Date::CurrentMonth,
                                MyMoneyReport::eDetailAll,
                                i18n("Monthly Budgeted vs. Actual"),
                                i18n("Generated Report"));

    reportCfg.setBudget("Any", true);

    reports::PivotTable table(reportCfg);

    PivotGrid grid = table.grid();

    //div header
    d->m_html += "<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">" + i18n("Budget") + "</div>\n<div class=\"gap\">&nbsp;</div>\n";

    //display budget summary
    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
    d->m_html += "<tr class=\"itemtitle\">";
    d->m_html += "<td class=\"left\" colspan=\"3\">";
    d->m_html += i18n("Current Month Summary");
    d->m_html += "</td></tr>";
    d->m_html += "<tr class=\"item\">";
    d->m_html += "<td class=\"right\" width=\"50%\">";
    d->m_html += i18n("Budgeted");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"20%\">";
    d->m_html += i18n("Actual");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"20%\">";
    d->m_html += i18n("Difference");
    d->m_html += "</td></tr>";

    d->m_html += QString("<tr class=\"row-odd\">");

    MyMoneyMoney totalBudgetValue = grid.m_total[eBudget].m_total;
    MyMoneyMoney totalActualValue = grid.m_total[eActual].m_total;
    MyMoneyMoney totalBudgetDiffValue = grid.m_total[eBudgetDiff].m_total;

    QString totalBudgetAmount = totalBudgetValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    QString totalActualAmount = totalActualValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
    QString totalBudgetDiffAmount = totalBudgetDiffValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);

    d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(totalBudgetAmount, totalBudgetValue.isNegative()));
    d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(totalActualAmount, totalActualValue.isNegative()));
    d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(totalBudgetDiffAmount, totalBudgetDiffValue.isNegative()));
    d->m_html += "</tr>";
    d->m_html += "</table>";

    //budget overrun
    d->m_html += "<div class=\"gap\">&nbsp;</div>\n";
    d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
    d->m_html += "<tr class=\"itemtitle\">";
    d->m_html += "<td class=\"left\" colspan=\"4\">";
    d->m_html += i18n("Budget Overruns");
    d->m_html += "</td></tr>";
    d->m_html += "<tr class=\"item\">";
    d->m_html += "<td class=\"left\" width=\"30%\">";
    d->m_html += i18n("Account");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"20%\">";
    d->m_html += i18n("Budgeted");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"20%\">";
    d->m_html += i18n("Actual");
    d->m_html += "</td>";
    d->m_html += "<td class=\"right\" width=\"20%\">";
    d->m_html += i18n("Difference");
    d->m_html += "</td></tr>";


    PivotGrid::iterator it_outergroup = grid.begin();
    while (it_outergroup != grid.end()) {
      i = 0;
      PivotOuterGroup::iterator it_innergroup = (*it_outergroup).begin();
      while (it_innergroup != (*it_outergroup).end()) {
        PivotInnerGroup::iterator it_row = (*it_innergroup).begin();
        while (it_row != (*it_innergroup).end()) {
          //column number is 1 because the report includes only current month
          if (it_row.value()[eBudgetDiff].value(1).isNegative()) {
            //get report account to get the name later
            ReportAccount rowname = it_row.key();

            //write the outergroup if it is the first row of outergroup being shown
            if (i == 0) {
              d->m_html += "<tr style=\"font-weight:bold;\">";
              d->m_html += QString("<td class=\"left\" colspan=\"4\">%1</td>").arg(MyMoneyAccount::accountTypeToString(rowname.accountType()));
              d->m_html += "</tr>";
            }
            d->m_html += QString("<tr class=\"row-%1\">").arg(i++ & 0x01 ? "even" : "odd");

            //get values from grid
            MyMoneyMoney actualValue = it_row.value()[eActual][1];
            MyMoneyMoney budgetValue = it_row.value()[eBudget][1];
            MyMoneyMoney budgetDiffValue = it_row.value()[eBudgetDiff][1];

            //format amounts
            QString actualAmount = actualValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
            QString budgetAmount = budgetValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
            QString budgetDiffAmount = budgetDiffValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);

            //account name
            d->m_html += QString("<td>") + link(VIEW_LEDGER, QString("?id=%1").arg(rowname.id())) + rowname.name() + linkend() + "</td>";

            //show amounts
            d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(budgetAmount, budgetValue.isNegative()));
            d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(actualAmount, actualValue.isNegative()));
            d->m_html += QString("<td align=\"right\">%1</td>").arg(showColoredAmount(budgetDiffAmount, budgetDiffValue.isNegative()));
            d->m_html += "</tr>";

            //set the flag that there are overruns
            isOverrun = true;
          }
          ++it_row;
        }
        ++it_innergroup;
      }
      ++it_outergroup;
    }

    //if no negative differences are found, then inform that
    if (!isOverrun) {
      d->m_html += QString("<tr class=\"row-%1\" style=\"font-weight:bold;\">").arg(i++ & 0x01 ? "even" : "odd");
      d->m_html += QString("<td class=\"center\" colspan=\"4\">%1</td>").arg(i18n("No Budget Categories have been overrun"));
      d->m_html += "</tr>";
    }
    d->m_html += "</table></div></div>";
  }
}

QString KHomeView::showColoredAmount(const QString& amount, bool isNegative)
{
  if (isNegative) {
    //if negative, get the settings for negative numbers
    return QString("<font color=\"%1\">%2</font>").arg(KMyMoneyGlobalSettings::schemeColor(SchemeColor::Negative).name(), amount);
  }

  //if positive, return the same string
  return amount;
}

void KHomeView::doForecast()
{
  //clear m_accountList because forecast is about to changed
  d->m_accountList.clear();

  //reinitialize the object
  d->m_forecast = KMyMoneyGlobalSettings::forecast();

  //If forecastDays lower than accountsCycle, adjust to the first cycle
  if (d->m_forecast.accountsCycle() > d->m_forecast.forecastDays())
    d->m_forecast.setForecastDays(d->m_forecast.accountsCycle());

  //Get all accounts of the right type to calculate forecast
  d->m_forecast.doForecast();
}

MyMoneyMoney KHomeView::forecastPaymentBalance(const MyMoneyAccount& acc, const MyMoneyMoney& payment, QDate& paymentDate)
{
  //if paymentDate before or equal to currentDate set it to current date plus 1
  //so we get to accumulate forecast balance correctly
  if (paymentDate <= QDate::currentDate())
    paymentDate = QDate::currentDate().addDays(1);

  //check if the account is already there
  if (d->m_accountList.find(acc.id()) == d->m_accountList.end()
      || d->m_accountList[acc.id()].find(paymentDate) == d->m_accountList[acc.id()].end()) {
    if (paymentDate == QDate::currentDate()) {
      d->m_accountList[acc.id()][paymentDate] = d->m_forecast.forecastBalance(acc, paymentDate);
    } else {
      d->m_accountList[acc.id()][paymentDate] = d->m_forecast.forecastBalance(acc, paymentDate.addDays(-1));
    }
  }
  d->m_accountList[acc.id()][paymentDate] = d->m_accountList[acc.id()][paymentDate] + payment;
  return d->m_accountList[acc.id()][paymentDate];
}

void KHomeView::showCashFlowSummary()
{
  MyMoneyTransactionFilter filter;
  MyMoneyMoney incomeValue;
  MyMoneyMoney expenseValue;

  MyMoneyFile* file = MyMoneyFile::instance();
  int prec = MyMoneyMoney::denomToPrec(file->baseCurrency().smallestAccountFraction());

  //set start and end of month dates
  QDate startOfMonth = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);
  QDate endOfMonth = QDate(QDate::currentDate().year(), QDate::currentDate().month(), QDate::currentDate().daysInMonth());

  //Add total income and expenses for this month
  //get transactions for current month
  filter.setDateFilter(startOfMonth, endOfMonth);
  filter.setReportAllSplits(false);

  QList<MyMoneyTransaction> transactions = file->transactionList(filter);
  //if no transaction then skip and print total in zero
  if (transactions.size() > 0) {
    QList<MyMoneyTransaction>::const_iterator it_transaction;

    //get all transactions for this month
    for (it_transaction = transactions.constBegin(); it_transaction != transactions.constEnd(); ++it_transaction) {

      //get the splits for each transaction
      const QList<MyMoneySplit>& splits = (*it_transaction).splits();
      QList<MyMoneySplit>::const_iterator it_split;
      for (it_split = splits.begin(); it_split != splits.end(); ++it_split) {
        if (!(*it_split).shares().isZero()) {
          ReportAccount repSplitAcc = ReportAccount((*it_split).accountId());

          //only add if it is an income or expense
          if (repSplitAcc.isIncomeExpense()) {
            MyMoneyMoney value;

            //convert to base currency if necessary
            if (repSplitAcc.currencyId() != file->baseCurrency().id()) {
              MyMoneyMoney curPrice = repSplitAcc.baseCurrencyPrice((*it_transaction).postDate());
              value = ((*it_split).shares() * MyMoneyMoney::MINUS_ONE) * curPrice;
              value = value.convert(10000);
            } else {
              value = ((*it_split).shares() * MyMoneyMoney::MINUS_ONE);
            }

            //store depending on account type
            if (repSplitAcc.accountType() == Account::Income) {
              incomeValue += value;
            } else {
              expenseValue += value;
            }
          }
        }
      }
    }
  }

  //format income and expenses
  QString amountIncome = incomeValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountExpense = expenseValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  amountIncome.replace(QChar(' '), "&nbsp;");
  amountExpense.replace(QChar(' '), "&nbsp;");

  //calculate schedules

  //Add all schedules for this month
  MyMoneyMoney scheduledIncome;
  MyMoneyMoney scheduledExpense;
  MyMoneyMoney scheduledLiquidTransfer;
  MyMoneyMoney scheduledOtherTransfer;

  //get overdues and schedules until the end of this month
  QList<MyMoneySchedule> schedule = file->scheduleList(QString(), Schedule::Type::Any,
                                    Schedule::Occurrence::Any,
                                    Schedule::PaymentType::Any,
                                    QDate(),
                                    endOfMonth, false);

  //Remove the finished schedules
  QList<MyMoneySchedule>::Iterator finished_it;
  for (finished_it = schedule.begin(); finished_it != schedule.end();) {
    if ((*finished_it).isFinished()) {
      finished_it = schedule.erase(finished_it);
      continue;
    }
    ++finished_it;
  }

  //add income and expenses
  QList<MyMoneySchedule>::Iterator sched_it;
  for (sched_it = schedule.begin(); sched_it != schedule.end();) {
    QDate nextDate = (*sched_it).nextDueDate();
    int cnt = 0;

    while (nextDate.isValid() && nextDate <= endOfMonth) {
      ++cnt;
      nextDate = (*sched_it).nextPayment(nextDate);
      // for single occurrence nextDate will not change, so we
      // better get out of here.
      if ((*sched_it).occurrence() == Schedule::Occurrence::Once)
        break;
    }

    MyMoneyAccount acc = (*sched_it).account();
    if (!acc.id().isEmpty()) {
      MyMoneyTransaction transaction = (*sched_it).transaction();
      // only show the entry, if it is still active

      MyMoneySplit sp = transaction.splitByAccount(acc.id(), true);

      // take care of the autoCalc stuff
      if ((*sched_it).type() == Schedule::Type::LoanPayment) {
        QDate nextDate = (*sched_it).nextPayment((*sched_it).lastPayment());

        //make sure we have all 'starting balances' so that the autocalc works
        QList<MyMoneySplit>::const_iterator it_s;
        QMap<QString, MyMoneyMoney> balanceMap;

        for (it_s = transaction.splits().constBegin(); it_s != transaction.splits().constEnd(); ++it_s) {
          MyMoneyAccount acc = file->account((*it_s).accountId());
          // collect all overdues on the first day
          QDate schedDate = nextDate;
          if (QDate::currentDate() >= nextDate)
            schedDate = QDate::currentDate().addDays(1);

          balanceMap[acc.id()] += file->balance(acc.id(), QDate::currentDate());
        }
        KMyMoneyUtils::calculateAutoLoan(*sched_it, transaction, balanceMap);
      }

      //go through the splits and assign to liquid or other transfers
      const QList<MyMoneySplit> splits = transaction.splits();
      QList<MyMoneySplit>::const_iterator split_it;
      for (split_it = splits.constBegin(); split_it != splits.constEnd(); ++split_it) {
        if ((*split_it).accountId() != acc.id()) {
          ReportAccount repSplitAcc = ReportAccount((*split_it).accountId());

          //get the shares and multiply by the quantity of occurrences in the period
          MyMoneyMoney value = (*split_it).shares() * cnt;

          //convert to foreign currency if needed
          if (repSplitAcc.currencyId() != file->baseCurrency().id()) {
            MyMoneyMoney curPrice = repSplitAcc.baseCurrencyPrice(QDate::currentDate());
            value = value * curPrice;
            value = value.convert(10000);
          }

          if ((repSplitAcc.isLiquidLiability()
               || repSplitAcc.isLiquidAsset())
              && acc.accountGroup() != repSplitAcc.accountGroup()) {
            scheduledLiquidTransfer += value;
          } else if (repSplitAcc.isAssetLiability()
                     && !repSplitAcc.isLiquidLiability()
                     && !repSplitAcc.isLiquidAsset()) {
            scheduledOtherTransfer += value;
          } else if (repSplitAcc.isIncomeExpense()) {
            //income and expenses are stored as negative values
            if (repSplitAcc.accountType() == Account::Income)
              scheduledIncome -= value;
            if (repSplitAcc.accountType() == Account::Expense)
              scheduledExpense -= value;
          }
        }
      }
    }
    ++sched_it;
  }

  //format the currency strings
  QString amountScheduledIncome = scheduledIncome.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountScheduledExpense = scheduledExpense.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountScheduledLiquidTransfer = scheduledLiquidTransfer.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountScheduledOtherTransfer = scheduledOtherTransfer.formatMoney(file->baseCurrency().tradingSymbol(), prec);

  amountScheduledIncome.replace(QChar(' '), "&nbsp;");
  amountScheduledExpense.replace(QChar(' '), "&nbsp;");
  amountScheduledLiquidTransfer.replace(QChar(' '), "&nbsp;");
  amountScheduledOtherTransfer.replace(QChar(' '), "&nbsp;");

  //get liquid assets and liabilities
  QList<MyMoneyAccount> accounts;
  QList<MyMoneyAccount>::const_iterator account_it;
  MyMoneyMoney liquidAssets;
  MyMoneyMoney liquidLiabilities;

  // get list of all accounts
  file->accountList(accounts);
  for (account_it = accounts.constBegin(); account_it != accounts.constEnd();) {
    if (!(*account_it).isClosed()) {
      switch ((*account_it).accountType()) {
          //group all assets into one list
        case Account::Checkings:
        case Account::Savings:
        case Account::Cash: {
            MyMoneyMoney value = MyMoneyFile::instance()->balance((*account_it).id(), QDate::currentDate());
            //calculate balance for foreign currency accounts
            if ((*account_it).currencyId() != file->baseCurrency().id()) {
              ReportAccount repAcc = ReportAccount((*account_it).id());
              MyMoneyMoney curPrice = repAcc.baseCurrencyPrice(QDate::currentDate());
              MyMoneyMoney baseValue = value * curPrice;
              liquidAssets += baseValue;
              liquidAssets = liquidAssets.convert(10000);
            } else {
              liquidAssets += value;
            }
            break;
          }
          //group the liabilities into the other
        case Account::CreditCard: {
            MyMoneyMoney value;
            value = MyMoneyFile::instance()->balance((*account_it).id(), QDate::currentDate());
            //calculate balance if foreign currency
            if ((*account_it).currencyId() != file->baseCurrency().id()) {
              ReportAccount repAcc = ReportAccount((*account_it).id());
              MyMoneyMoney curPrice = repAcc.baseCurrencyPrice(QDate::currentDate());
              MyMoneyMoney baseValue = value * curPrice;
              liquidLiabilities += baseValue;
              liquidLiabilities = liquidLiabilities.convert(10000);
            } else {
              liquidLiabilities += value;
            }
            break;
          }
        default:
          break;
      }
    }
    ++account_it;
  }
  //calculate net worth
  MyMoneyMoney liquidWorth = liquidAssets + liquidLiabilities;

  //format assets, liabilities and net worth
  QString amountLiquidAssets = liquidAssets.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountLiquidLiabilities = liquidLiabilities.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountLiquidWorth = liquidWorth.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  amountLiquidAssets.replace(QChar(' '), "&nbsp;");
  amountLiquidLiabilities.replace(QChar(' '), "&nbsp;");
  amountLiquidWorth.replace(QChar(' '), "&nbsp;");

  //show the summary
  d->m_html += "<div class=\"shadow\"><div class=\"displayblock\"><div class=\"summaryheader\">" + i18n("Cash Flow Summary") + "</div>\n<div class=\"gap\">&nbsp;</div>\n";

  //print header
  d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
  //income and expense title
  d->m_html += "<tr class=\"itemtitle\">";
  d->m_html += "<td class=\"left\" colspan=\"4\">";
  d->m_html += i18n("Income and Expenses of Current Month");
  d->m_html += "</td></tr>";
  //column titles
  d->m_html += "<tr class=\"item\">";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Income");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Scheduled Income");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Expenses");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Scheduled Expenses");
  d->m_html += "</td>";
  d->m_html += "</tr>";

  //add row with banding
  d->m_html += QString("<tr class=\"row-even\" style=\"font-weight:bold;\">");

  //print current income
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountIncome, incomeValue.isNegative()));

  //print the scheduled income
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountScheduledIncome, scheduledIncome.isNegative()));

  //print current expenses
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountExpense,  expenseValue.isNegative()));

  //print the scheduled expenses
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountScheduledExpense,  scheduledExpense.isNegative()));
  d->m_html += "</tr>";

  d->m_html += "</table>";

  //print header of assets and liabilities
  d->m_html += "<div class=\"gap\">&nbsp;</div>\n";
  d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
  //assets and liabilities title
  d->m_html += "<tr class=\"itemtitle\">";
  d->m_html += "<td class=\"left\" colspan=\"4\">";
  d->m_html += i18n("Liquid Assets and Liabilities");
  d->m_html += "</td></tr>";
  //column titles
  d->m_html += "<tr class=\"item\">";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Liquid Assets");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Transfers to Liquid Liabilities");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Liquid Liabilities");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Other Transfers");
  d->m_html += "</td>";
  d->m_html += "</tr>";

  //add row with banding
  d->m_html += QString("<tr class=\"row-even\" style=\"font-weight:bold;\">");

  //print current liquid assets
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountLiquidAssets, liquidAssets.isNegative()));

  //print the scheduled transfers
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountScheduledLiquidTransfer, scheduledLiquidTransfer.isNegative()));

  //print current liabilities
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountLiquidLiabilities,  liquidLiabilities.isNegative()));

  //print the scheduled transfers
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountScheduledOtherTransfer, scheduledOtherTransfer.isNegative()));


  d->m_html += "</tr>";

  d->m_html += "</table>";

  //final conclusion
  MyMoneyMoney profitValue = incomeValue + expenseValue + scheduledIncome + scheduledExpense;
  MyMoneyMoney expectedAsset = liquidAssets + scheduledIncome + scheduledExpense + scheduledLiquidTransfer + scheduledOtherTransfer;
  MyMoneyMoney expectedLiabilities = liquidLiabilities + scheduledLiquidTransfer;

  QString amountExpectedAsset = expectedAsset.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountExpectedLiabilities = expectedLiabilities.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  QString amountProfit = profitValue.formatMoney(file->baseCurrency().tradingSymbol(), prec);
  amountProfit.replace(QChar(' '), "&nbsp;");
  amountExpectedAsset.replace(QChar(' '), "&nbsp;");
  amountExpectedLiabilities.replace(QChar(' '), "&nbsp;");



  //print header of cash flow status
  d->m_html += "<div class=\"gap\">&nbsp;</div>\n";
  d->m_html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" class=\"summarytable\" >";
  //income and expense title
  d->m_html += "<tr class=\"itemtitle\">";
  d->m_html += "<td class=\"left\" colspan=\"4\">";
  d->m_html += i18n("Cash Flow Status");
  d->m_html += "</td></tr>";
  //column titles
  d->m_html += "<tr class=\"item\">";
  d->m_html += "<td>&nbsp;</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Expected Liquid Assets");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Expected Liquid Liabilities");
  d->m_html += "</td>";
  d->m_html += "<td width=\"25%\" class=\"center\">";
  d->m_html += i18n("Expected Profit/Loss");
  d->m_html += "</td>";
  d->m_html += "</tr>";

  //add row with banding
  d->m_html += QString("<tr class=\"row-even\" style=\"font-weight:bold;\">");
  d->m_html += "<td>&nbsp;</td>";

  //print expected assets
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountExpectedAsset, expectedAsset.isNegative()));

  //print expected liabilities
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountExpectedLiabilities, expectedLiabilities.isNegative()));

  //print expected profit
  d->m_html += QString("<td align=\"right\">%2</td>").arg(showColoredAmount(amountProfit, profitValue.isNegative()));

  d->m_html += "</tr>";
  d->m_html += "</table>";
  d->m_html += "</div></div>";
}

// Make sure, that these definitions are only used within this file
// this does not seem to be necessary, but when building RPMs the
// build option 'final' is used and all CPP files are concatenated.
// So it could well be, that in another CPP file these definitions
// are also used.
#undef VIEW_LEDGER
#undef VIEW_SCHEDULE
#undef VIEW_WELCOME
#undef VIEW_HOME
#undef VIEW_REPORTS
