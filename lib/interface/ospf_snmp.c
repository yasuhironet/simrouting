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

#ifdef HAVE_NETSNMP

#include "ospf.h"
#include "snmp.h"
#include "ospf_snmp.h"

static void
ospf_lsa_oid_name (char *name, int len, char *area,
                   u_char *type, void *id, void *adv_router)
{
  char buf[16];

  /* initialize */
  memset (name, 0, len);

  /* header part */
  strncat (name, "OSPF-MIB::", len);
  strncat (name, "ospfLsdbAdvertisement.", len - strlen (name));
  strncat (name, area, len - strlen (name));

  /* return here if type is not specified */
  if (type == NULL)
    return;

  /* add type string */
  if (*type == OSPF_LSTYPE_ROUTER)
    strncat (name, ".routerLink", len - strlen (name));
  else
    strncat (name, ".networkLink", len - strlen (name));

  /* return here if id is not specified */
  if (id == NULL)
    return;

  /* add id string */
  strncat (name, ".", len - strlen (name));
  inet_ntop (AF_INET, id, buf, sizeof (buf));
  strncat (name, buf, len - strlen (name));

  /* return here if adv_router is not specified */
  if (adv_router == NULL)
    return;

  /* add adv_router string */
  strncat (name, ".", len - strlen (name));
  inet_ntop (AF_INET, adv_router, buf, sizeof (buf));
  strncat (name, buf, len - strlen (name));
}

struct ospf_lsa *
ospf_get_lsa_head (struct snmp_session *sess, char *area, u_char type)
{
  char name[OSPF_MAX_OID_NAME_LEN];
  char buf[OSPF_MAX_LSA_SIZE];
  int buflen = OSPF_MAX_LSA_SIZE;
  struct ospf_lsa *lsa;
  u_char typespec = type;

  ospf_lsa_oid_name (name, sizeof (name), area, &typespec, NULL, NULL);
  snmp_get_next (sess, name, TYPE_RESTRICT_LEN, buf, &buflen);
  if (buflen == 0)
    return NULL;

  lsa = ospf_lsa_create (buf, buflen);

  /* if we went out of range, free the LSA and return NULL */
  if (lsa->header.type != type)
    {
      ospf_lsa_delete (lsa);
      return NULL;
    }

  return lsa;
}

struct ospf_lsa *
ospf_get_lsa_next (struct snmp_session *sess, char *area,
                   struct ospf_lsa *prev)
{
  char name[OSPF_MAX_OID_NAME_LEN];
  char buf[OSPF_MAX_LSA_SIZE];
  int buflen = OSPF_MAX_LSA_SIZE;
  struct ospf_lsa *lsa;

  ospf_lsa_oid_name (name, sizeof (name), area,
                     &prev->header.type, &prev->header.id,
                     &prev->header.adv_router);
  snmp_get_next (sess, name, TYPE_RESTRICT_LEN, buf, &buflen);
  if (buflen == 0)
    return NULL;

  lsa = ospf_lsa_create (buf, buflen);

  if (lsa->header.type != prev->header.type)
    {
      ospf_lsa_delete (lsa);
      return NULL;
    }

  return lsa;
}

struct table *
ospf_lsdb_get_by_snmp (char *host, char *community, char *area)
{
  struct snmp_session *sess;
  struct ospf_lsa *lsa;
  char id[16], adv_router[16];
  u_char *key;
  struct table *lsdb;

  lsdb = table_create ();

  sess = snmp_start (SNMP_VERSION_2c, host, community);

  for (lsa = ospf_get_lsa_head (sess, area, OSPF_LSTYPE_ROUTER); lsa;
       lsa = ospf_get_lsa_next (sess, area, lsa))
    {
      inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
      inet_ntop (AF_INET, &lsa->header.adv_router, adv_router,
                 sizeof (adv_router));
      printf ("// Get LSA: Type-%d id: %s adv_router: %s\n",
              lsa->header.type, id, adv_router);

      key = ospf_lsdb_key (lsa->header.type, lsa->header.id,
                           lsa->header.adv_router);
      table_add (key, LSDB_KEY_LEN, lsa, lsdb);
    }

  for (lsa = ospf_get_lsa_head (sess, area, OSPF_LSTYPE_NETWORK); lsa;
       lsa = ospf_get_lsa_next (sess, area, lsa))
    {
      inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
      inet_ntop (AF_INET, &lsa->header.adv_router, adv_router,
                 sizeof (adv_router));
      printf ("// Get LSA: Type-%d id: %s adv_router: %s\n",
              lsa->header.type, id, adv_router);

      key = ospf_lsdb_key (lsa->header.type, lsa->header.id,
                           lsa->header.adv_router);
      table_add (key, LSDB_KEY_LEN, lsa, lsdb);
    }

  return lsdb;
}

struct graph *
graph_import_ospf (struct table *lsdb)
{
  unsigned int index = 0;
  struct node *v;
  struct table_node *node;
  int i;
  struct graph *graph;

  struct table *router_table;
  struct table *network_table;

  graph = graph_create ();

  router_table = table_create ();
  network_table = table_create ();

  for (node = table_head (lsdb); node; node = table_next (node))
    {
      struct ospf_lsa *lsa = node->data;

      v = node_get (index, graph);
      assert (v);

      v->addr = ospf_lsa_ipaddr (lsa);
      v->plen = ospf_lsa_prefixlen (lsa);
      v->type = lsa->header.type;
      v->name = strdup (ospf_lsa_name (lsa));
      v->domain_name = strdup (ospf_lsa_fqdn (lsa));

      if (lsa->header.type == OSPF_LSTYPE_ROUTER)
        table_add ((u_char *)&lsa->header.adv_router, 32,
                   v, router_table);
      else if (lsa->header.type == OSPF_LSTYPE_NETWORK)
        table_add ((u_char *)&lsa->header.id, 32,
                   v, network_table);

      index++;
    }

  for (node = table_head (lsdb); node; node = table_next (node))
    {
      struct ospf_lsa *lsa = node->data;
      struct node *this, *peer;

      if (lsa->header.type == OSPF_LSTYPE_ROUTER)
        {
          struct router_lsa *router_lsa;
          struct router_lsa_link *link;

          this = table_lookup ((u_char *)&lsa->header.adv_router,
                               32, router_table);

          router_lsa = (struct router_lsa *)
            ((caddr_t)lsa + sizeof (struct ospf_lsa_header));

          for (i = 0; i < ntohs (router_lsa->links); i++)
            {
              link = (struct router_lsa_link *) &router_lsa->link[i];

              switch (link->m[0].type)
                {
                case OSPF_LINK_TYPE_POINTOPOINT:
                case OSPF_LINK_TYPE_VIRTUALLINK:
                  peer = table_lookup ((u_char *)&link->link_id,
                                       32, router_table);
                  break;
                case OSPF_LINK_TYPE_TRANSIT:
                  peer = table_lookup ((u_char *)&link->link_id,
                                       32, network_table);
                  break;
                case OSPF_LINK_TYPE_STUB:
                  peer = NULL;
                  break;
                default:
                  peer = NULL;
                  fprintf (stderr, "Unknown Link in LSA %s\n",
                           ospf_lsa_name (lsa));
                  break;
                }

              if (peer)
                link_set (this, peer, graph);
            }
        }
      else if (lsa->header.type == OSPF_LSTYPE_NETWORK)
        {
          struct network_lsa *network_lsa;
          int size;

          this = table_lookup ((u_char *)&lsa->header.id,
                               32, network_table);

          network_lsa = (struct network_lsa *)
            ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));

          size = ntohs (lsa->header.length) -
                 sizeof (struct ospf_lsa_header);
          size -= sizeof (struct in_addr);
          size /= sizeof (struct in_addr);

          for (i = 0; i < size; i++)
            {
              peer = table_lookup ((u_char *)&network_lsa->routers[i],
                                   32, router_table);
              if (peer)
                link_set (this, peer, graph);
            }
        }
    }

  table_delete (router_table);
  table_delete (network_table);

  return graph;
}

struct weight *
weight_import_ospf (struct table *lsdb, struct graph *graph)
{
  struct vector_node *vn1, *vn2;
  struct node *v, *w;
  struct link *e;
  struct ospf_lsa *lsa;
  struct router_lsa *router_lsa;
  struct router_lsa_link *link;
  int i;
  struct weight *weight;
  u_char *key;

  weight = weight_create ();
  weight_setting_graph (weight, graph);

  for (vn1 = vector_head (graph->nodes); vn1; vn1 = vector_next (vn1))
    {
      v = (struct node *) vn1->data;

      if (v->type == OSPF_LSTYPE_NETWORK)
        continue;

      key = ospf_lsdb_key (v->type, v->addr, v->addr);
      lsa = table_lookup (key, LSDB_KEY_LEN, lsdb);
      assert (lsa);

      for (vn2 = vector_head (v->olinks); vn2; vn2 = vector_next (vn2))
        {
          e = (struct link *) vn2->data;
          assert (e->from == v);
          w = e->to;

          router_lsa = (struct router_lsa *)
            ((caddr_t)lsa + sizeof (struct ospf_lsa_header));

          for (i = 0; i < ntohs (router_lsa->links); i++)
            {
              link = (struct router_lsa_link *) &router_lsa->link[i];

              switch (link->m[0].type)
                {
                case OSPF_LINK_TYPE_POINTOPOINT:
                case OSPF_LINK_TYPE_VIRTUALLINK:
                  if (w->type == OSPF_LSTYPE_ROUTER &&
                      ! memcmp (&w->addr, &link->link_id,
                                sizeof (struct in_addr)))
                    weight->weight[e->id] = ntohs (link->m[0].metric);
                  break;
                case OSPF_LINK_TYPE_TRANSIT:
                  if (w->type == OSPF_LSTYPE_NETWORK &&
                      ! memcmp (&w->addr, &link->link_id,
                                sizeof (struct in_addr)))
                    weight->weight[e->id] = ntohs (link->m[0].metric);
                  break;
                case OSPF_LINK_TYPE_STUB:
                  break;
                default:
                  fprintf (stderr, "Unknown Link in LSA %s\n",
                           ospf_lsa_name (lsa));
                  assert (0);
                  break;
                }
            }

          assert (weight->weight[e->id]);
        }
    }
}


#endif /*HAVE_NETSNMP*/
