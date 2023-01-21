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

#ifndef _OSPF_H_
#define _OSPF_H_

#define OSPF_MAX_LSA_SIZE       1500

#define OSPF_LSTYPE_ROUTER  1
#define OSPF_LSTYPE_NETWORK 2

#define LSDB_KEY_LEN 72

struct ospf_lsa_header
{
  u_int16_t age;
  u_char options;
  u_char type;
  struct in_addr id;
  struct in_addr adv_router;
  int seqnum;
  u_int16_t cksum;
  u_int16_t length;
};

struct ospf_lsa
{
  struct ospf_lsa_header header;
};

struct tos_data
{
  u_char type;
  u_char tos_count;
  u_int16_t metric;
};

struct router_lsa_link
{
  struct in_addr link_id;
  struct in_addr link_data;
  struct tos_data m[1];
};

struct router_lsa
{
  u_char flags;
  u_char zero;
  u_int16_t links;
  struct router_lsa_link link[1];
};

#define OSPF_LINK_TYPE_POINTOPOINT      1
#define OSPF_LINK_TYPE_TRANSIT          2
#define OSPF_LINK_TYPE_STUB             3
#define OSPF_LINK_TYPE_VIRTUALLINK      4

struct network_lsa
{
  struct in_addr mask;
  struct in_addr routers[1];
};

char *ospf_lsa_name (struct ospf_lsa *lsa);
char *ospf_lsa_fqdn (struct ospf_lsa *lsa);
struct in_addr ospf_lsa_ipaddr (struct ospf_lsa *lsa);
u_char prefixlen_of_mask (struct in_addr netmask);
unsigned int ospf_lsa_prefixlen (struct ospf_lsa *lsa);
struct ospf_lsa *ospf_lsa_create (char *buf, int len);
void ospf_lsa_delete (struct ospf_lsa *lsa);
u_char *ospf_lsdb_key (u_char type, struct in_addr id,
                       struct in_addr adv_router);

#endif /*_OSPF_H_*/

