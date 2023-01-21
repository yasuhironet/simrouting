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
prefix_get (char *pstr, struct in_addr *in, unsigned int *len)
{
  char *p;
  p = strchr (pstr, '/');
  if (p)
    {
      *p++ = '\0';
      *len = (unsigned int) strtol (p, NULL, 10);
    }
  inet_pton (AF_INET, pstr, &in);
}


