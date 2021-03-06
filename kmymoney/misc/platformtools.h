/*
 * This file is part of KMyMoney, A Personal Finance Manager by KDE
 * Copyright (C) 2017 Marc Hübner <mahueb55@gmail.com>
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

#ifndef PLATFORMTOOLS_H
#define PLATFORMTOOLS_H

#include <sys/types.h>

class QString;


namespace platformTools
{

  /**
   * This function returns the os username of the user account
   * under which the application is being run.
   */
  QString osUsername();

  /**
   * This function returns the PID associated with the current process.
   */
  uint processId();
};

#endif // PLATFORMTOOLS_H
