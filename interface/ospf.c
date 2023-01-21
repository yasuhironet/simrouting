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

#include "shell.h"
#include "command.h"
#include "command_shell.h"

#include "network/graph.h"
#include "network/graph_cmd.h"
#include "table.h"
#include "ospf.h"
#include "snmp.h"

#include "prefix.h"
#include "network/weight.h"

#ifdef HAVE_NETSNMP

#define OSPF_MAX_OID_NAME_LEN    256

#define AREA_RESTRICT_LEN  14
#define TYPE_RESTRICT_LEN  15

struct snmp_target snmp_target;
char *lsdb_area;
struct table *lsdb;

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

void
ospf_lsa_oid_name (u_char *type, void *id, void *adv_router, char *area,
                   char *name, int namelen)
{
  char buf[16];
  memset (name, 0, namelen);
  strncat (name, "OSPF-MIB::", namelen);
  strncat (name, "ospfLsdbAdvertisement.", namelen - strlen (name));
  strncat (name, area, namelen - strlen (name));

  if (type == NULL)
    return;

  if (*type == OSPF_LSTYPE_ROUTER)
    strncat (name, ".routerLink", namelen - strlen (name));
  else
    strncat (name, ".networkLink", namelen - strlen (name));

  if (id == NULL)
    return;

  strncat (name, ".", namelen - strlen (name));
  inet_ntop (AF_INET, id, buf, sizeof (buf));
  strncat (name, buf, namelen - strlen (name));

  if (adv_router == NULL)
    return;

  strncat (name, ".", namelen - strlen (name));
  inet_ntop (AF_INET, adv_router, buf, sizeof (buf));
  strncat (name, buf, namelen - strlen (name));
}

struct ospf_lsa *
ospf_get_lsa_head (u_char type, char *area, struct snmp_target *target)
{
  char name[OSPF_MAX_OID_NAME_LEN];
  char buf[OSPF_MAX_LSA_SIZE];
  int buflen = OSPF_MAX_LSA_SIZE;
  struct ospf_lsa *lsa;
  u_char typespec = type;

  ospf_lsa_oid_name (&typespec, NULL, NULL, area, name, sizeof (name));
  snmp_get_next (target, name, TYPE_RESTRICT_LEN, buf, &buflen);
  if (buflen == 0)
    return NULL;

  lsa = ospf_lsa_create (buf, buflen);
  if (lsa->header.type != type)
    {
      ospf_lsa_delete (lsa);
      return NULL;
    }

  return lsa;
}

struct ospf_lsa *
ospf_get_lsa_next (struct ospf_lsa *prev, char *area, struct snmp_target *target)
{
  char name[OSPF_MAX_OID_NAME_LEN];
  char buf[OSPF_MAX_LSA_SIZE];
  int buflen = OSPF_MAX_LSA_SIZE;
  struct ospf_lsa *lsa;

  ospf_lsa_oid_name (&prev->header.type, &prev->header.id,
                     &prev->header.adv_router, area,
                     name, sizeof (name));
  snmp_get_next (target, name, TYPE_RESTRICT_LEN, buf, &buflen);
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

u_char *
lsdb_key (u_char type, struct in_addr id, struct in_addr adv_router)
{
  static u_char buf[16];
  memcpy (&buf[0], &type, sizeof (u_char));
  memcpy (&buf[1], &adv_router, sizeof (struct in_addr));
  memcpy (&buf[5], &id, sizeof (struct in_addr));
  return buf;
}

#define LSDB_KEY_LEN 72

void
ospf_lsdb_get (char *host, char *community, char *area)
{
  struct ospf_lsa *lsa;
  char id[16], adv_router[16];

  if (lsdb)
    table_delete (lsdb);
  lsdb = table_create ();

  if (snmp_target.host)
    free (snmp_target.host);
  if (snmp_target.community)
    free (snmp_target.community);
  if (lsdb_area)
    free (lsdb_area);

  snmp_target.host = strdup (host);
  snmp_target.community = strdup (community);
  lsdb_area = strdup (area);

  for (lsa = ospf_get_lsa_head (OSPF_LSTYPE_ROUTER, area, &snmp_target); lsa;
       lsa = ospf_get_lsa_next (lsa, area, &snmp_target))
    {
      inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
      inet_ntop (AF_INET, &lsa->header.adv_router, adv_router, sizeof (adv_router));
      fprintf (stdout, "// Get LSA: Type-%d id: %s adv_router: %s\n",
               lsa->header.type, id, adv_router);
      table_add (lsdb_key (lsa->header.type, lsa->header.id, lsa->header.adv_router),
                LSDB_KEY_LEN, lsa, lsdb);
    }

  for (lsa = ospf_get_lsa_head (OSPF_LSTYPE_NETWORK, area, &snmp_target); lsa;
       lsa = ospf_get_lsa_next (lsa, area, &snmp_target))
    {
      inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
      inet_ntop (AF_INET, &lsa->header.adv_router, adv_router,
                 sizeof (adv_router));
      fprintf (stdout, "// Get LSA: Type-%d id: %s adv_router: %s\n",
               lsa->header.type, id, adv_router);
      table_add (lsdb_key (lsa->header.type, lsa->header.id, lsa->header.adv_router),
                LSDB_KEY_LEN, lsa, lsdb);
    }

  fflush (stdout);
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

char *
lsa_name (struct ospf_lsa *lsa)
{
  static char buf[64];
  char id[16], adv_router[16];
  inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header.adv_router, adv_router, sizeof (adv_router));
  snprintf (buf, sizeof (buf), "type-%d id: %s adv_router: %s",
            lsa->header.type, id, adv_router);
  return buf;
}

char *
get_fqdn (struct ospf_lsa *lsa, char *name, int size)
{
  int ret;
  struct sockaddr_in sin;

  memset (&sin, 0, sizeof (struct sockaddr_in));
#ifdef HAVE_SIN_LEN
  sin.sin_len = sizeof (struct sockaddr_in);
#endif /*HAVE_SIN_LEN*/
  sin.sin_family = AF_INET;

  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    sin.sin_addr = lsa->header.adv_router;
  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    sin.sin_addr = lsa->header.id;

  ret = getnameinfo ((struct sockaddr *) &sin, sizeof (struct sockaddr_in),
                     name, size, NULL, 0, NI_NAMEREQD);
  if (ret != 0)
    {
      fprintf (stdout, "%s: getnameinfo (): error: %d (%s)\n",
               lsa_name (lsa), ret, gai_strerror (ret));
      fflush (stdout);
    }

  return name;
}

void
node_name_resolve (struct ospf_lsa *lsa, char *name, int size)
{
  char addrname[16], nodename[32], dnsname[32];

  memset (addrname, 0, sizeof (addrname));
  memset (nodename, 0, sizeof (nodename));
  memset (dnsname, 0, sizeof (dnsname));

  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    {
      inet_ntop (AF_INET, &lsa->header.adv_router, nodename, sizeof (nodename));
    }
  else
    {
      struct network_lsa *network_lsa = (struct network_lsa *)
        ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));
      inet_ntop (AF_INET, &lsa->header.id, addrname, sizeof (addrname));
      snprintf (nodename, sizeof (nodename), "%s/%d",
                addrname, prefixlen_of_mask (network_lsa->mask));
    }

  get_fqdn (lsa, dnsname, sizeof (dnsname));

  if (strlen (dnsname))
    snprintf (name, size, "%s (%s)", nodename, dnsname);
  else
    snprintf (name, size, "%s", nodename);
}

struct in_addr
node_addr (struct ospf_lsa *lsa)
{
  struct in_addr addr;
  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    addr = lsa->header.adv_router;
  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    addr = lsa->header.id;
  return addr;
}

unsigned int
node_plen (struct ospf_lsa *lsa)
{
  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    return 32;

  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    {
      struct network_lsa *network_lsa = (struct network_lsa *)
        ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));
      return ((unsigned int) prefixlen_of_mask (network_lsa->mask));
    }

  return 0;
}

char *
node_name (struct ospf_lsa *lsa)
{
  static char nodename[32];
  char addrname[16];

  memset (addrname, 0, sizeof (addrname));
  memset (nodename, 0, sizeof (nodename));

  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    {
      inet_ntop (AF_INET, &lsa->header.adv_router, nodename, sizeof (nodename));
    }

  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    {
      struct network_lsa *network_lsa = (struct network_lsa *)
        ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));
      inet_ntop (AF_INET, &lsa->header.id, addrname, sizeof (addrname));
      snprintf (nodename, sizeof (nodename), "%s/%d",
                addrname, prefixlen_of_mask (network_lsa->mask));
    }

  return strdup (nodename);
}

char *
node_domain_name (struct ospf_lsa *lsa)
{
  static char domainname[64];
  int ret;
  struct sockaddr_in sin;

  memset (domainname, 0, sizeof (domainname));
  memset (&sin, 0, sizeof (struct sockaddr_in));

#ifdef HAVE_SIN_LEN
  sin.sin_len = sizeof (struct sockaddr_in);
#endif /*HAVE_SIN_LEN*/
  sin.sin_family = AF_INET;

  if (lsa->header.type == OSPF_LSTYPE_ROUTER)
    sin.sin_addr = lsa->header.adv_router;
  if (lsa->header.type == OSPF_LSTYPE_NETWORK)
    sin.sin_addr = lsa->header.id;

  ret = getnameinfo ((struct sockaddr *) &sin, sizeof (struct sockaddr_in),
                     domainname, sizeof (domainname), NULL, 0, NI_NAMEREQD);
  if (ret != 0)
    {
      fprintf (stderr, "%s: getnameinfo (): error: %d (%s)\n",
               lsa_name (lsa), ret, gai_strerror (ret));
      fflush (stderr);
      return NULL;
    }

  return strdup (domainname);
}


void
read_ospf_lsdb (struct table *lsdb, struct graph *G)
{
  unsigned int index = 0;
  struct node *v;
  struct table_node *node;
  int i;

  struct table *router_table;
  struct table *network_table;

  router_table = table_create ();
  network_table = table_create ();

  for (node = table_head (lsdb); node; node = table_next (node))
    {
      struct ospf_lsa *lsa = node->data;

      v = node_get (index, G);
      assert (v);

      v->addr = node_addr (lsa);
      v->plen = node_plen (lsa);
      v->type = lsa->header.type;
      v->name = node_name (lsa);
      v->domain_name = node_domain_name (lsa);

      if (lsa->header.type == OSPF_LSTYPE_ROUTER)
        table_add ((u_char *)&lsa->header.adv_router, 32, v, router_table);
      else if (lsa->header.type == OSPF_LSTYPE_NETWORK)
        table_add ((u_char *)&lsa->header.id, 32, v, network_table);

      index++;
    }

  for (node = table_head (lsdb); node; node = table_next (node))
    {
      struct ospf_lsa *lsa = node->data;
      struct node *this, *peer;

      char id[16], adv_router[16];
      inet_ntop (AF_INET, &lsa->header.id, id, sizeof (id));
      inet_ntop (AF_INET, &lsa->header.adv_router, adv_router, sizeof (adv_router));
      fprintf (stdout, "type-%d id: %s adv_router: %s\n",
               lsa->header.type, id, adv_router);

      if (lsa->header.type == OSPF_LSTYPE_ROUTER)
        {
          struct router_lsa *router_lsa;
          struct router_lsa_link *link;

          this = table_lookup ((u_char *)&lsa->header.adv_router, 32, router_table);

          router_lsa = (struct router_lsa *)
            ((caddr_t)lsa + sizeof (struct ospf_lsa_header));

          for (i = 0; i < ntohs (router_lsa->links); i++)
            {
              link = (struct router_lsa_link *) &router_lsa->link[i];

              switch (link->m[0].type)
                {
                case OSPF_LINK_TYPE_POINTOPOINT:
                case OSPF_LINK_TYPE_VIRTUALLINK:
                  peer = table_lookup ((u_char *)&link->link_id, 32, router_table);
                  break;
                case OSPF_LINK_TYPE_TRANSIT:
                  peer = table_lookup ((u_char *)&link->link_id, 32, network_table);
                  break;
                case OSPF_LINK_TYPE_STUB:
                  peer = NULL;
                  break;
                default:
                  peer = NULL;
                  fprintf (stderr, "Unknown Link in LSA %s\n", lsa_name (lsa));
                  break;
                }

              if (peer)
                link_set (this, peer, G);
            }
        }
      else if (lsa->header.type == OSPF_LSTYPE_NETWORK)
        {
          struct network_lsa *network_lsa;
          int size;

          this = table_lookup ((u_char *)&lsa->header.id, 32, network_table);

          network_lsa = (struct network_lsa *)
            ((caddr_t)&lsa->header + sizeof (struct ospf_lsa_header));

          size = ntohs (lsa->header.length) - sizeof (struct ospf_lsa_header);
          size -= sizeof (struct in_addr);
          size /= sizeof (struct in_addr);

          for (i = 0; i < size; i++)
            {
              peer = table_lookup ((u_char *)&network_lsa->routers[i], 32, router_table);
              if (peer)
                link_set (this, peer, G);
            }
        }
    }

  table_delete (router_table);
  table_delete (network_table);

  fflush (stdout);
}

void
import_ospf_graph (struct shell *shell, struct graph *G,
                   char *host, char *community, char *area)
{
  ospf_lsdb_get (host, community, area);
  read_ospf_lsdb (lsdb, G);
}

void
read_ospf_lsdb_metric (struct table *lsdb, struct graph *G, struct weight *W)
{
  struct vector_node *vn1, *vn2;
  struct node *v, *w;
  struct link *e;
  struct ospf_lsa *lsa;
  struct router_lsa *router_lsa;
  struct router_lsa_link *link;
  int i;

  for (vn1 = vector_head (G->nodes); vn1; vn1 = vector_next (vn1))
    {
      v = (struct node *) vn1->data;

      if (v->type == OSPF_LSTYPE_NETWORK)
        continue;

      lsa = table_lookup (lsdb_key (v->type, v->addr, v->addr),
                          LSDB_KEY_LEN, lsdb);
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
                    W->weight[e->id] = ntohs (link->m[0].metric);
                  break;
                case OSPF_LINK_TYPE_TRANSIT:
                  if (w->type == OSPF_LSTYPE_NETWORK &&
                      ! memcmp (&w->addr, &link->link_id,
                                sizeof (struct in_addr)))
                    W->weight[e->id] = ntohs (link->m[0].metric);
                  break;
                case OSPF_LINK_TYPE_STUB:
                  break;
                default:
                  fprintf (stderr, "Unknown Link in LSA %s\n", lsa_name (lsa));
                  assert (0);
                  break;
                }
            }

          assert (W->weight[e->id]);
        }
    }
}

#endif /*HAVE_NETSNMP*/

DEFINE_COMMAND(import_ospf_graph,
               "import ospf HOST COMMUNITY A.B.C.D",
               "import from other data\n"
               "import from OSPF LSDB via SNMP\n"
               "specify SNMP HOST\n"
               "specify SNMP COMMUNITY\n"
               "specify OSPF area\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
#ifdef HAVE_NETSNMP
  import_ospf_graph (shell, graph, argv[2], argv[3], argv[4]);
#else /*HAVE_NETSNMP*/
  fprintf (stderr, "SNMP is not supported.\n");
#endif /*HAVE_NETSNMP*/
  graph_save_config (graph);
}

DEFINE_COMMAND(import_ospf_weight,
               "import ospf HOST COMMUNITY A.B.C.D",
               "import from other data\n"
               "import from OSPF LSDB via SNMP\n"
               "specify SNMP HOST\n"
               "specify SNMP COMMUNITY\n"
               "specify OSPF area\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *weight = (struct weight *) shell->context;
  struct graph *graph = weight->G;
#ifdef HAVE_NETSNMP
  if (strcmp (snmp_target.host, argv[2]) ||
      strcmp (snmp_target.community, argv[3]) ||
      strcmp (lsdb_area, argv[4]))
    {
      fprintf (shell->terminal, "different SNMP target.\n");
      return;
    }
  read_ospf_lsdb_metric (lsdb, graph, weight);
#else /*HAVE_NETSNMP*/
  fprintf (stderr, "SNMP is not supported.\n");
#endif /*HAVE_NETSNMP*/
  weight_save_config (weight);
}

#if 0

void
import_ospf_graph_match_node (struct shell *shell, struct graph *G,
                              char *host, char *community, char *area,
                              struct table *ruleset)
{
  struct table_node *node;
  struct table_node *n;
  int match;
  struct in_addr in;
  int len;
  struct ospf_lsa *lsa;

  ospf_lsdb_get (host, community, area);

  fprintf (shell->terminal, "begin\n");
  for (node = table_head (lsdb); node; node = table_next (node))
    {
      lsa = (struct ospf_lsa *) node->data;
      fprintf (shell->terminal, "matching %s\n", lsa_name (lsa));

      match = 0;
      for (n = table_head (ruleset); n; n = table_next (n))
        {
          char *rule = (char *) n->data;

          if (! strncmp (rule, "router ", 7) &&
              lsa->header.type == OSPF_LSTYPE_ROUTER)
            {
              prefix_get (rule + 7, &in, &len);
              if (key_match ((u_char *)&lsa->header.adv_router, (u_char *)&in, len))
                {
                  fprintf (shell->terminal, "matched rule %d\n",
                           *(unsigned short *)&n->key);
                  match++;
                }
            }

          if (! strncmp (rule, "network ", 8) &&
              lsa->header.type == OSPF_LSTYPE_NETWORK)
            {
              prefix_get (rule + 8, &in, &len);
              if (key_match ((u_char *)&lsa->header.id, (u_char *)&in, len))
                match++;
            }

          if (! strncmp (rule, "domain ", 7))
            {
              struct in_addr addr;
              char buf[64];

              if (lsa->header.type == OSPF_LSTYPE_ROUTER)
                addr = lsa->header.adv_router;
              if (lsa->header.type == OSPF_LSTYPE_NETWORK)
                addr = lsa->header.id;

              get_fqdn (lsa, buf, sizeof (buf));
              if (strlen (buf) &&
                  strstr (buf, rule + 7))
                match++;
            }
        }

      if (! match)
        {
          fprintf (shell->terminal, "ignore no-match %s\n", lsa_name (lsa));
          table_remove (lsdb_key (lsa->header.type, lsa->header.id,
                                 lsa->header.adv_router),
                       LSDB_KEY_LEN, lsa, lsdb);
        }
    }
  fprintf (shell->terminal, "end\n");
  fflush (shell->terminal);

  read_ospf_lsdb (lsdb, G);
}

struct table *rule_table;

DEFINE_COMMAND(import_rule_add_router,
               "import rule RULE <0-65535> router A.B.C.D/M",
               "import from other data\n"
               "specify rule for importing\n"
               "specify RULE name\n"
               "specify RULE id\n"
               "specify router rule\n"
               "specify address range which contains Advertising Router-IDs\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  char rule_string[64];
  unsigned short rule_id;
  struct table *rule;

  if (! rule_table)
    rule_table = table_create ();

  snprintf (rule_string, sizeof (rule_string), "%s %s", argv[4], argv[5]);

  rule = table_create ();
  rule_id = (unsigned short) strtol (argv[3], NULL, 10);
  table_add ((u_char *)&rule_id, sizeof (unsigned short), strdup (rule_string), rule);

  table_add ((u_char *)argv[2], strlen (argv[2]), rule, rule_table);
  command_config_add (G->config, argc, argv);
}

DEFINE_COMMAND(import_ospf_graph_with_rule,
               "import ospf HOST COMMUNITY A.B.C.D with rule RULE",
               "import from other data\n"
               "import from OSPF LSDB via SNMP\n"
               "specify SNMP HOST\n"
               "specify SNMP COMMUNITY\n"
               "specify OSPF area\n"
               "import graph nodes which match rules\n"
               "import graph nodes which match rules\n"
               "specify RULE name\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct table *rule;

  if (! rule_table)
    return;

  rule = (struct table *) table_lookup ((u_char *)argv[7], strlen (argv[7]), rule_table);

  import_ospf_graph_match_node (shell, G, argv[2], argv[3], argv[4], rule);
  command_config_add (G->config, argc, argv);
}

#endif /*0*/


