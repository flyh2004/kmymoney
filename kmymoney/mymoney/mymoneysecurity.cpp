/***************************************************************************
                          mymoneysecurity.cpp  -  description
                             -------------------
    begin                : Tue Jan 29 2002
    copyright            : (C) 2000-2002 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
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

#include "mymoneysecurity.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QString>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyexception.h"
#include "mymoneystoragenames.h"

using namespace eMyMoney;
using namespace MyMoneyStorageNodes;

class MyMoneySecurityPrivate {

public:

  MyMoneySecurityPrivate() :
    m_securityType(eMyMoney::Security::None),
    m_smallestCashFraction(MyMoneySecurity::DEFAULT_CASH_FRACTION),
    m_smallestAccountFraction(MyMoneySecurity::DEFAULT_ACCOUNT_FRACTION),
    m_pricePrecision(MyMoneySecurity::DEFAULT_PRICE_PRECISION),
    m_roundingMethod(AlkValue::RoundRound)
  {
  }

  QString                   m_name;
  QString                   m_tradingSymbol;
  QString                   m_tradingMarket;
  QString                   m_tradingCurrency;
  eMyMoney::Security        m_securityType;
  int                       m_smallestCashFraction;
  int                       m_smallestAccountFraction;
  int                       m_pricePrecision;
  AlkValue::RoundingMethod  m_roundingMethod;
};

MyMoneySecurity::MyMoneySecurity() :
  d_ptr(new MyMoneySecurityPrivate)
{
}

MyMoneySecurity::MyMoneySecurity(const QString& id,
                                 const QString& name,
                                 const QString& symbol,
                                 const int smallestCashFraction,
                                 const int smallestAccountFraction,
                                 const int pricePrecision) :
  MyMoneyObject(id),
  MyMoneyKeyValueContainer(),
  d_ptr(new MyMoneySecurityPrivate)
{
  Q_D(MyMoneySecurity);
  d->m_name = name;
  d->m_smallestCashFraction = smallestCashFraction;
  d->m_pricePrecision = pricePrecision;
  d->m_securityType = eMyMoney::Security::Currency;

  if (symbol.isEmpty())
    d->m_tradingSymbol = id;
  else
    d->m_tradingSymbol = symbol;

  if (smallestAccountFraction)
    d->m_smallestAccountFraction = smallestAccountFraction;
  else
    d->m_smallestAccountFraction = smallestCashFraction;
}

MyMoneySecurity::MyMoneySecurity(const QDomElement& node) :
  MyMoneyObject(node),
  MyMoneyKeyValueContainer(node.elementsByTagName(nodeNames[nnKeyValuePairs]).item(0).toElement()),
  d_ptr(new MyMoneySecurityPrivate)
{
  {
    const auto tag = node.tagName();
    if ((nodeNames[nnSecurity] != tag)
        && (nodeNames[nnEquity] != tag)
        && (nodeNames[nnCurrency] != tag))
      throw MYMONEYEXCEPTION("Node was not SECURITY or CURRENCY");
  }

  Q_D(MyMoneySecurity);
  d->m_name = node.attribute(getAttrName(Attribute::Name));
  d->m_tradingSymbol = node.attribute(getAttrName(Attribute::Symbol));
  d->m_securityType = static_cast<eMyMoney::Security>(node.attribute(getAttrName(Attribute::Type)).toInt());
  d->m_roundingMethod = static_cast<AlkValue::RoundingMethod>(node.attribute(getAttrName(Attribute::RoundingMethod)).toInt());
  d->m_smallestAccountFraction = node.attribute(getAttrName(Attribute::SAF)).toUInt();
  d->m_pricePrecision = node.attribute(getAttrName(Attribute::PP)).toUInt();

  if (d->m_smallestAccountFraction == 0)
    d->m_smallestAccountFraction = 100;
  if (d->m_pricePrecision == 0 || d->m_pricePrecision > 10)
    d->m_pricePrecision = 4;

  if (isCurrency()) {
    d->m_smallestCashFraction = node.attribute(getAttrName(Attribute::SCF)).toUInt();
    if (d->m_smallestCashFraction == 0)
      d->m_smallestCashFraction = 100;
  } else {
    d->m_tradingCurrency = node.attribute(getAttrName(Attribute::TradingCurrency));
    d->m_tradingMarket = node.attribute(getAttrName(Attribute::TradingMarket));
  }
}

MyMoneySecurity::MyMoneySecurity(const MyMoneySecurity& other) :
  MyMoneyObject(other.id()),
  MyMoneyKeyValueContainer(other),
  d_ptr(new MyMoneySecurityPrivate(*other.d_func()))
{
}

MyMoneySecurity::MyMoneySecurity(const QString& id, const MyMoneySecurity& other) :
  MyMoneyObject(id),
  MyMoneyKeyValueContainer(other),
  d_ptr(new MyMoneySecurityPrivate(*other.d_func()))
{
}

MyMoneySecurity::~MyMoneySecurity()
{
  Q_D(MyMoneySecurity);
  delete d;
}

bool MyMoneySecurity::operator == (const MyMoneySecurity& right) const
{
  Q_D(const MyMoneySecurity);
  auto d2 = static_cast<const MyMoneySecurityPrivate *>(right.d_func());
  return (m_id == right.m_id)
         && (d->m_name == d2->m_name)
         && (d->m_tradingSymbol == d2->m_tradingSymbol)
         && (d->m_tradingMarket == d2->m_tradingMarket)
         && (d->m_roundingMethod == d2->m_roundingMethod)
         && (d->m_tradingSymbol == d2->m_tradingSymbol)
         && (d->m_tradingCurrency == d2->m_tradingCurrency)
         && (d->m_securityType == d2->m_securityType)
         && (d->m_smallestAccountFraction == d2->m_smallestAccountFraction)
         && (d->m_smallestCashFraction == d2->m_smallestCashFraction)
         && (d->m_pricePrecision == d2->m_pricePrecision)
         && this->MyMoneyKeyValueContainer::operator == (right);
}

bool MyMoneySecurity::operator < (const MyMoneySecurity& right) const
{
  Q_D(const MyMoneySecurity);
  auto d2 = static_cast<const MyMoneySecurityPrivate *>(right.d_func());
  if (d->m_securityType == d2->m_securityType)
    return d->m_name < d2->m_name;
  return d->m_securityType < d2->m_securityType;
}

QString MyMoneySecurity::name() const
{
  Q_D(const MyMoneySecurity);
  return d->m_name;
}

void MyMoneySecurity::setName(const QString& str)
{
  Q_D(MyMoneySecurity);
  d->m_name = str;
}

QString MyMoneySecurity::tradingSymbol() const
{
  Q_D(const MyMoneySecurity);
  return d->m_tradingSymbol;
}

void MyMoneySecurity::setTradingSymbol(const QString& str)
{
  Q_D(MyMoneySecurity);
  d->m_tradingSymbol = str;
}

QString MyMoneySecurity::tradingMarket() const
{
  Q_D(const MyMoneySecurity);
  return d->m_tradingMarket;
}

void MyMoneySecurity::setTradingMarket(const QString& str)
{
  Q_D(MyMoneySecurity);
  d->m_tradingMarket = str;
}

QString MyMoneySecurity::tradingCurrency() const
{
  Q_D(const MyMoneySecurity);
  return d->m_tradingCurrency;
}

void MyMoneySecurity::setTradingCurrency(const QString& str)
{
  Q_D(MyMoneySecurity);
  d->m_tradingCurrency = str;
}

bool MyMoneySecurity::operator != (const MyMoneySecurity& r) const
{
  return !(*this == r);
}

eMyMoney::Security MyMoneySecurity::securityType() const
{
  Q_D(const MyMoneySecurity);
  return d->m_securityType;
}

void MyMoneySecurity::setSecurityType(const eMyMoney::Security s)
{
  Q_D(MyMoneySecurity);
  d->m_securityType = s;
}

bool MyMoneySecurity::isCurrency() const
{
  Q_D(const MyMoneySecurity);
  return d->m_securityType == eMyMoney::Security::Currency;
}

AlkValue::RoundingMethod MyMoneySecurity::roundingMethod() const
{
  Q_D(const MyMoneySecurity);
  return d->m_roundingMethod;
}

void MyMoneySecurity::setRoundingMethod(const AlkValue::RoundingMethod rnd)
{
  Q_D(MyMoneySecurity);
  d->m_roundingMethod = rnd;
}

int MyMoneySecurity::smallestAccountFraction() const
{
  Q_D(const MyMoneySecurity);
  return d->m_smallestAccountFraction;
}

void MyMoneySecurity::setSmallestAccountFraction(const int sf)
{
  Q_D(MyMoneySecurity);
  d->m_smallestAccountFraction = sf;
}

int MyMoneySecurity::smallestCashFraction() const
{
  Q_D(const MyMoneySecurity);
  return d->m_smallestCashFraction;
}

void MyMoneySecurity::setSmallestCashFraction(const int cf)
{
  Q_D(MyMoneySecurity);
  d->m_smallestCashFraction = cf;
}

int MyMoneySecurity::pricePrecision() const
{
  Q_D(const MyMoneySecurity);
  return d->m_pricePrecision;
}

void MyMoneySecurity::setPricePrecision(const int pp)
{
  Q_D(MyMoneySecurity);
  d->m_pricePrecision = pp;
}

bool MyMoneySecurity::hasReferenceTo(const QString& id) const
{
  Q_D(const MyMoneySecurity);
  return (id == d->m_tradingCurrency);
}

void MyMoneySecurity::writeXML(QDomDocument& document, QDomElement& parent) const
{
  QDomElement el;
  if (isCurrency())
    el = document.createElement(nodeNames[nnCurrency]);
  else
    el = document.createElement(nodeNames[nnSecurity]);

  writeBaseXML(document, el);

  Q_D(const MyMoneySecurity);
  el.setAttribute(getAttrName(Attribute::Name), d->m_name);
  el.setAttribute(getAttrName(Attribute::Symbol),d->m_tradingSymbol);
  el.setAttribute(getAttrName(Attribute::Type), static_cast<int>(d->m_securityType));
  el.setAttribute(getAttrName(Attribute::RoundingMethod), static_cast<int>(d->m_roundingMethod));
  el.setAttribute(getAttrName(Attribute::SAF), d->m_smallestAccountFraction);
  el.setAttribute(getAttrName(Attribute::PP), d->m_pricePrecision);
  if (isCurrency())
    el.setAttribute(getAttrName(Attribute::SCF), d->m_smallestCashFraction);
  else {
    el.setAttribute(getAttrName(Attribute::TradingCurrency), d->m_tradingCurrency);
    el.setAttribute(getAttrName(Attribute::TradingMarket), d->m_tradingMarket);
  }

  //Add in Key-Value Pairs for securities.
  MyMoneyKeyValueContainer::writeXML(document, el);

  parent.appendChild(el);
}

QString MyMoneySecurity::securityTypeToString(const eMyMoney::Security securityType)
{
  switch (securityType) {
    case eMyMoney::Security::Stock:
      return i18nc("Security type", "Stock");
    case eMyMoney::Security::MutualFund:
      return i18nc("Security type", "Mutual Fund");
    case eMyMoney::Security::Bond:
      return i18nc("Security type", "Bond");
    case eMyMoney::Security::Currency:
      return i18nc("Security type", "Currency");
    case eMyMoney::Security::None:
      return i18nc("Security type", "None");
    default:
      return i18nc("Security type", "Unknown");
  }
}

QString MyMoneySecurity::roundingMethodToString(const AlkValue::RoundingMethod roundingMethod)
{
  switch (roundingMethod) {
    case AlkValue::RoundNever:
      return I18N_NOOP("Never");
    case AlkValue::RoundFloor:
      return I18N_NOOP("Floor");
    case AlkValue::RoundCeil:
      return I18N_NOOP("Ceil");
    case AlkValue::RoundTruncate:
      return I18N_NOOP("Truncate");
    case AlkValue::RoundPromote:
      return I18N_NOOP("Promote");
    case AlkValue::RoundHalfDown:
      return I18N_NOOP("HalfDown");
    case AlkValue::RoundHalfUp:
      return I18N_NOOP("HalfUp");
    case AlkValue::RoundRound:
      return I18N_NOOP("Round");
    default:
      return I18N_NOOP("Unknown");
  }
}

QString MyMoneySecurity::getAttrName(const Attribute attr)
{
  static const QHash<Attribute, QString> attrNames = {
    {Attribute::Name,             QStringLiteral("name")},
    {Attribute::Symbol,           QStringLiteral("symbol")},
    {Attribute::Type,             QStringLiteral("type")},
    {Attribute::RoundingMethod,   QStringLiteral("rounding-method")},
    {Attribute::SAF,              QStringLiteral("saf")},
    {Attribute::PP,               QStringLiteral("pp")},
    {Attribute::SCF,              QStringLiteral("scf")},
    {Attribute::TradingCurrency,  QStringLiteral("trading-currency")},
    {Attribute::TradingMarket,    QStringLiteral("trading-market")}
  };
  return attrNames[attr];
}
