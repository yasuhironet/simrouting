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

#ifndef _GROUP_H_
#define _GROUP_H_

struct group_entry
{
  int result;
  int type;
  void *arg;
};

#define NO_MATCH 0
#define MATCH    1

#define DOMAIN_SUFFIX   1
#define ROUTER_ID_RANGE 2
#define NETWORK_ID      3

int group_match (struct node *node, char *group_name);

void group_config_write (FILE *out);
void group_init ();
void group_finish ();

#endif /*_GROUP_H_*/

