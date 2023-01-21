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

void
gettimeofday_sub (struct timeval *start, struct timeval *end,
                  struct timeval *res)
{
  res->tv_sec = end->tv_sec - start->tv_sec
                - (end->tv_usec < start->tv_usec ? 1 : 0);
  res->tv_usec = (end->tv_usec < start->tv_usec ? 1000000 : 0)
                 + end->tv_usec - start->tv_usec;
}

inline unsigned long long rdtsc() {
    unsigned long long ret;
    __asm__ volatile ("rdtsc" : "=A" (ret));
    return ret;
}


