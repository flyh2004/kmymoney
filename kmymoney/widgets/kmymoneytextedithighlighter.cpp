/*
  This file is part of KMyMoney, A Personal Finance Manager by KDE
  Copyright (C) 2013 Christian Dávid <christian-david@web.de>
  (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "kmymoneytextedithighlighter.h"

/* The Higligther */

class KMyMoneyTextEditHighlighterPrivate
{
  Q_DISABLE_COPY(KMyMoneyTextEditHighlighterPrivate)

public:
  KMyMoneyTextEditHighlighterPrivate() :
    m_allowedChars(QString(QString())),
    m_maxLines(-1),
    m_maxLineLength(-1),
    m_maxLength(-1)
  {
  }

  QString m_allowedChars;
  int m_maxLines;
  int m_maxLineLength;
  int m_maxLength;
};


KMyMoneyTextEditHighlighter::KMyMoneyTextEditHighlighter(QTextEdit * parent) :
  Highlighter(parent),
  d_ptr(new KMyMoneyTextEditHighlighterPrivate)
{
}

KMyMoneyTextEditHighlighter::~KMyMoneyTextEditHighlighter()
{
  Q_D(KMyMoneyTextEditHighlighter);
  delete d;
}

void KMyMoneyTextEditHighlighter::setAllowedChars(const QString& chars)
{
  m_allowedChars = chars;
  rehighlight();
}

void KMyMoneyTextEditHighlighter::setMaxLength(const int& length)
{
  m_maxLength = length;
  rehighlight();
}

void KMyMoneyTextEditHighlighter::setMaxLines(const int& lines)
{
  m_maxLines = lines;
  rehighlight();
}

void KMyMoneyTextEditHighlighter::setMaxLineLength(const int& length)
{
  m_maxLineLength = length;
  rehighlight();
}

void KMyMoneyTextEditHighlighter::highlightBlock(const QString& text)
{
  // Spell checker first
  Highlighter::highlightBlock(text);

  QTextCharFormat invalidFormat;
  invalidFormat.setFontItalic(true);
  invalidFormat.setForeground(Qt::red);
  invalidFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

  // Check used characters
  const int length = text.length();
  for (auto i = 0; i < length; ++i) {
    if (!m_allowedChars.contains(text.at(i))) {
      setFormat(i, 1, invalidFormat);
    }
  }

  if (m_maxLines != -1) {
    //! @todo Is using QTextBlock::blockNumber() as line number dangerous?
    if (currentBlock().blockNumber() >= m_maxLines) {
      setFormat(0, length, invalidFormat);
      return;
    }
  }

  if (m_maxLength != -1) {
    const int blockPosition = currentBlock().position();
    if (m_maxLength < (length + blockPosition)) {
      setFormat(m_maxLength, length - m_maxLength - blockPosition, invalidFormat);
      return;
    }
  }

  if (m_maxLineLength != -1 && length >= m_maxLineLength) {
    setFormat(m_maxLineLength, length - m_maxLineLength, invalidFormat);
    return;
  }
}
