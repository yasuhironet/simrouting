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

#ifndef _OSPF_SNMP_H_
#define _OSPF_SNMP_H_

#define OSPF_MAX_OID_NAME_LEN    256

#define AREA_RESTRICT_LEN  14
#define TYPE_RESTRICT_LEN  15

struct table *
ospf_lsdb_get_by_snmp (char *host, char *community, char *area);

struct graph *graph_import_ospf (struct table *lsdb);
struct weight *
weight_import_ospf (struct table *lsdb, struct graph *graph);

#endif /*_OSPF_SNMP_H_*/

