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

#include "table.h"
#include "prefix.h"

#include "network/graph.h"
#include "network/weight.h"

#include "ospf.h"

char *
ospf_lsa_name (struct ospf_lsa *lsa)
{
  static char buf[64];
  char id[16], adv_router[16];
  inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header.adv_router, adv_router,
             sizeof (adv_router));
  snprintf (buf, sizeof (buf), "type-%d id: %s adv_router: %s",
            lsa->header.type, id, adv_router);
  return buf;
}

char *
ospf_lsa_fqdn (struct ospf_lsa *lsa)
{
  static char buf[64];
  struct sockaddr_in sin;
  int ret;

  memset (&sin, 0, sizeof (struct sockaddr_in));
#ifdef HAVE_SIN_LEN
  sin.sin_len = sizeof (struct sockaddr_in);
#endif /*HAVE_SIN_LEN*/
  sin.sin_family = AF_INET;

  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    sin.sin_addr = lsa->header.adv_router;
  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    sin.sin_addr = lsa->header.id;

  ret = getnameinfo ((struct sockaddr *) &sin,
                     sizeof (struct sockaddr_in),
                     buf, sizeof (buf), NULL, 0, NI_NAMEREQD);
  if (ret != 0)
    {
      fprintf (stdout, "%s: getnameinfo (): error: %d (%s)\n",
               ospf_lsa_name (lsa), ret, gai_strerror (ret));
      fflush (stdout);
    }

  return buf;
}

struct in_addr
ospf_lsa_ipaddr (struct ospf_lsa *lsa)
{
  struct in_addr addr;
  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    addr = lsa->header.adv_router;
  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    addr = lsa->header.id;
  return addr;
}

u_char
prefixlen_of_mask (struct in_addr netmask)
{
  u_char len = 0;
  u_char *pnt, *end;

  pnt = (u_char *) &netmask;
  end = pnt + sizeof (netmask);
  while (pnt < end && (*pnt == 0xff))
    {
      len += 8;
      pnt ++;
    }

  if (pnt < end)
    {
      u_char val = *pnt;
      while (val)
        {
          len ++;
          val <<= 1;
        }
    }

  return len;
}

unsigned int
ospf_lsa_prefixlen (struct ospf_lsa *lsa)
{
  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    return 32;

  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    {
      struct network_lsa *network_lsa;
      network_lsa  = (struct network_lsa *)
        ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));
      return ((unsigned int) prefixlen_of_mask (network_lsa->mask));
    }

  return 0;
}

struct ospf_lsa *
ospf_lsa_create (char *buf, int len)
{
  struct ospf_lsa_header *header = (struct ospf_lsa_header *) buf;
  unsigned short length = ntohs (header->length);
  struct ospf_lsa *lsa;

  lsa = (struct ospf_lsa *) malloc (length);
  memset (lsa, 0, length);
  memcpy (lsa, buf, length);
  return lsa;
}

void
ospf_lsa_delete (struct ospf_lsa *lsa)
{
  free (lsa);
}

u_char *
ospf_lsdb_key (u_char type, struct in_addr id, struct in_addr adv_router)
{
  static u_char buf[16];
  memcpy (&buf[0], &type, sizeof (u_char));
  memcpy (&buf[1], &adv_router, sizeof (struct in_addr));
  memcpy (&buf[5], &id, sizeof (struct in_addr));
  return buf;
}


