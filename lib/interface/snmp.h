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

#ifndef _SNMP_H_
#define _SNMP_H_

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

struct snmp_target
{
  int version;
  char *host;
  char *community;
};

struct snmp_session *
snmp_start (int version, char *host, char *community);

void
snmp_get_next (struct snmp_session *sess,
               char *name, int restrict_len,
               char *buf, int *buflen);

void
snmp_end (struct snmp_session *sess);

#endif /*_SNMP_H_*/

