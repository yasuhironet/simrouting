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
#include "table.h"

#include "network/graph.h"
#include "network/group.h"

#include "prefix.h"

struct table *groups = NULL;
struct vector *group_config = NULL;

struct group_entry *
group_entry_create ()
{
  struct group_entry *entry;
  entry = (struct group_entry *) malloc (sizeof (struct group_entry));
  memset (entry, 0, sizeof (struct group_entry));
  return entry;
}

void
group_entry_delete (struct group_entry *entry)
{
  if (entry->arg)
    free (entry->arg);
  free (entry);
}

DEFINE_COMMAND(group_match_domain_suffix,
               "group GROUPNAME <0-65535> match domain-suffix DOMAIN",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return match\n"
               "matching with domain-name suffix\n"
               "specify domain name suffix\n")
{
  unsigned short entry_id;
  struct group_entry *entry;
  struct table *group_table;

  group_table = table_lookup (argv[1], strlen (argv[1]) * 8, groups);
  if (group_table == NULL)
    {
      group_table = table_create ();
      table_add (argv[1], strlen (argv[1]) * 8, group_table, groups);
    }
  entry_id = (unsigned short) strtol (argv[2], NULL, 10);
  entry = table_lookup (&entry_id, 16, group_table);
  if (entry == NULL)
    entry = group_entry_create ();
  if (! strcmp (argv[3], "match"))
    entry->result = MATCH;
  else
    entry->result = NO_MATCH;
  entry->type = DOMAIN_SUFFIX;
  entry->arg = strdup (argv[5]);
  table_add (&entry_id, 16, entry, group_table);

  command_config_add (group_config, argc, argv);
}

ALIAS_COMMAND(group_no_match_domain_suffix,
              group_match_domain_suffix,
               "group GROUPNAME <0-65535> no-match domain-suffix DOMAIN",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return no-match\n"
               "exclusive matching with domain-name suffix\n"
               "specify domain name suffix\n");

DEFINE_COMMAND(group_match_router_id_range,
               "group GROUPNAME <0-65535> match router-id-range A.B.C.D/M",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return match\n"
               "matching with router-id range\n"
               "specify router-id range as an IP prefix\n")
{
  unsigned short entry_id;
  struct group_entry *entry;
  struct prefix *prefix;
  struct table *group_table;

  group_table = table_lookup (argv[1], strlen (argv[1]) * 8, groups);
  if (group_table == NULL)
    {
      group_table = table_create ();
      table_add (argv[1], strlen (argv[1]) * 8, group_table, groups);
    }
  entry_id = (unsigned short) strtol (argv[2], NULL, 10);
  entry = table_lookup (&entry_id, 16, group_table);
  if (entry == NULL)
    entry = group_entry_create ();
  if (! strcmp (argv[3], "match"))
    entry->result = MATCH;
  else
    entry->result = NO_MATCH;
  entry->type = ROUTER_ID_RANGE;
  prefix = (struct prefix *) malloc (sizeof (struct prefix));
  memset (prefix, 0, sizeof (prefix));
  prefix_get (argv[5], &prefix->addr, &prefix->plen);
  entry->arg = prefix;
  table_add (&entry_id, 16, entry, group_table);

  command_config_add (group_config, argc, argv);
}

ALIAS_COMMAND(group_no_match_router_id_range,
              group_match_router_id_range,
               "group GROUPNAME <0-65535> no-match router-id-range A.B.C.D/M",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return no-match\n"
               "exclusive matching with router-id range\n"
               "specify router-id range as an IP prefix\n");

DEFINE_COMMAND(group_match_network_id,
               "group GROUPNAME <0-65535> match network-id A.B.C.D/M",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return match\n"
               "matching with network-id\n"
               "specify network-id as an IP prefix\n")
{
  unsigned short entry_id;
  struct group_entry *entry;
  struct prefix *prefix;
  struct table *group_table;

  group_table = table_lookup (argv[1], strlen (argv[1]) * 8, groups);
  if (group_table == NULL)
    {
      group_table = table_create ();
      table_add (argv[1], strlen (argv[1]) * 8, group_table, groups);
    }
  entry_id = (unsigned short) strtol (argv[2], NULL, 10);
  entry = table_lookup (&entry_id, 16, group_table);
  if (entry == NULL)
    entry = group_entry_create ();
  if (! strcmp (argv[3], "match"))
    entry->result = MATCH;
  else
    entry->result = NO_MATCH;
  entry->type = NETWORK_ID;
  prefix = (struct prefix *) malloc (sizeof (struct prefix));
  memset (prefix, 0, sizeof (prefix));
  prefix_get (argv[5], &prefix->addr, &prefix->plen);
  entry->arg = prefix;
  table_add (&entry_id, 16, entry, group_table);

  command_config_add (group_config, argc, argv);
}

ALIAS_COMMAND(group_no_match_network_id,
               group_match_network_id,
               "group GROUPNAME <0-65535> no-match network-id A.B.C.D/M",
               "group specify command\n"
               "specify group name\n"
               "specify number of this entry\n"
               "return no-match\n"
               "exclusive matching with network-id\n"
               "specify network-id as an IP prefix\n");

int
prefix_match (void *val, struct prefix *prefix)
{
  return key_match (val, &prefix->addr, prefix->plen);
}

int
group_match (struct node *node, char *group_name)
{
  struct table *group_table;
  struct table_node *tn;
  char *p;
  struct group_entry *entry;
  struct prefix *prefix;

  group_table = table_lookup (group_name, strlen (group_name) * 8, groups);
  if (group_table == NULL)
    return NO_MATCH;

  for (tn = table_head (group_table); tn; tn = table_next (tn))
    {
      entry = (struct group_entry *) tn->data;
      switch (entry->type)
        {
        case DOMAIN_SUFFIX:
          if (node->domain_name == NULL)
            break;
          p = strstr (node->domain_name, entry->arg);
          if (p == NULL)
            break;
          if (! strcmp (p, entry->arg))
            return entry->result;
          break;

        case ROUTER_ID_RANGE:
          prefix = (struct prefix *) entry->arg;
          if (prefix_match (&node->addr, prefix))
            return entry->result;
          break;

        case NETWORK_ID:
          prefix = (struct prefix *) entry->arg;
          if (memcmp (&node->addr, &prefix->addr, sizeof (struct in_addr)) &&
              node->plen == prefix->plen)
            return entry->result;
          break;
        default:
          break;
        }
    }
  return NO_MATCH;
}

void
group_config_write (FILE *out)
{
  fprintf (out, "\n");
  command_config_write (group_config, out);
}

void
group_init ()
{
  groups = table_create ();
  group_config = vector_create ();
  INSTALL_COMMAND (cmdset_default, group_match_domain_suffix);
  INSTALL_COMMAND (cmdset_default, group_no_match_domain_suffix);
  INSTALL_COMMAND (cmdset_default, group_match_router_id_range);
  INSTALL_COMMAND (cmdset_default, group_no_match_router_id_range);
  INSTALL_COMMAND (cmdset_default, group_match_network_id);
  INSTALL_COMMAND (cmdset_default, group_no_match_network_id);
}

void
group_finish ()
{
  struct table_node *n, *m;
  struct table *group_table;
  struct group_entry *entry;

  for (n = table_head (groups); n; n = table_next (n))
    {
      group_table = (struct table *) n->data;
      for (m = table_head (group_table); m; m = table_next (m))
        {
          entry = (struct group_entry *) m->data;
          group_entry_delete (entry);
        }
      table_delete (group_table);
    }
  table_delete (groups);

  command_config_clear (group_config);
  vector_delete (group_config);
}


