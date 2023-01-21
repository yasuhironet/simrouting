/*
 * Copyright (C) 2007  Yasuhiro Ohara
 * 
 * This file is part of SimRouting.
 * 
 * SimRouting is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * SimRouting is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <includes.h>

#include "log.h"

unsigned int __seed = 0;

void
random_init (unsigned int s)
{
  struct timeval tv;

  if (__seed != 0)
    return;

  if (s == 0)
    {
      gettimeofday (&tv, (struct timezone *) NULL);
      s = tv.tv_sec;
    }

  __seed = s;
  srandom (__seed);

  log_notice ("random init: seed: %u", __seed);
}

