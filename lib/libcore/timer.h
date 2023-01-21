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

#ifndef _TIMER_H_
#define _TIMER_H_

void gettimeofday_sub (struct timeval *start, struct timeval *end,
                       struct timeval *res);
unsigned long long rdtsc ();

#define TIMEVAL_TO_USEC(x) \
  ((x).tv_sec * 1000000 + (x).tv_usec)

#define CPU_COUNTER_TO_NSEC(count, clock) \
  ((count) * (unsigned long long)1e+9 / (clock))
#define CPU_COUNTER_TO_USEC(count, clock) \
  ((count) * (unsigned long long)1e+6 / (clock))
#define CPU_COUNTER_TO_MSEC(count, clock) \
  ((count) * (unsigned long long)1e+3 / (clock))
#define CPU_COUNTER_TO_SEC(count, clock) ((count) / (clock))

#define GETTIMEOFDAY 1
#define RDTSC        2

#define CPUHZ 2446133482

#define TIMER_TYPE RDTSC

#if TIMER_TYPE == GETTIMEOFDAY
  typedef struct timeval timer_counter_t;
  #define timer_count(count) (gettimeofday (&(count), NULL))
  #define timer_sub(start, end, res) \
    (gettimeofday_sub (&(start), &(end), &(res)))
  #define timer_to_usec(x) ((unsigned long long) TIMEVAL_TO_USEC (x))
#elif TIMER_TYPE == RDTSC
  typedef unsigned long long timer_counter_t;
  #define timer_count(count) (count = rdtsc ())
  #define timer_sub(start, end, res) ((res) = (end) - (start))
  #define timer_to_usec(res) (CPU_COUNTER_TO_USEC (res, CPUHZ))
#endif

#endif /*_TIMEER_H_*/


