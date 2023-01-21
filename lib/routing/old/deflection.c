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

#include "vector.h"

#include "file.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"

#include "table.h"

#include "traffic-model/demand.h"
#include "network/network.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/dijkstra.h"
#include "routing/deflection.h"

struct prev_hop_entry *
prev_hop_entry_create ()
{
  struct prev_hop_entry *entry;
  entry = (struct prev_hop_entry *) malloc (sizeof (struct prev_hop_entry));
  memset (entry, 0, sizeof (struct prev_hop_entry));

  entry->deflection_set_2 = vector_create ();
  entry->deflection_set_3 = vector_create ();

  return entry;
}

void
prev_hop_entry_delete (struct prev_hop_entry *entry)
{
  vector_delete (entry->deflection_set_2);
  vector_delete (entry->deflection_set_3);
  free (entry);
}

struct deflection_info *
deflection_info_create ()
{
  struct deflection_info *dinfo;

  dinfo = (struct deflection_info *) malloc (sizeof (struct deflection_info));
  memset (dinfo, 0, sizeof (struct deflection_info));

  return dinfo;
}

void
deflection_info_delete (struct deflection_info *dinfo)
{
  int i;
  struct vector_node *vn;

  for (i = 0; i < dinfo->nnodes; i++)
    candidate_delete (dinfo->table[i]);
  free (dinfo->table);

  for (i = 0; i < dinfo->nnodes; i++)
    {
      vector_delete (dinfo->deflection_set_1[i]);

      for (vn = vector_head (dinfo->prev_hop_table[i]); vn; vn = vector_next (vn))
        {
          struct prev_hop_entry *entry = (struct prev_hop_entry *) vn->data;
          prev_hop_entry_delete (entry);
        }
      vector_delete (dinfo->prev_hop_table[i]);
    }

  free (dinfo);
}

void
dinfo_table_delete (void *data)
{
  int i;
  int nnodes;
  struct deflection_info **dinfo_table = (struct deflection_info **) data;

  nnodes = dinfo_table[0]->nnodes;
  for (i = 0; i < nnodes; i++)
    deflection_info_delete (dinfo_table[i]);

  free (dinfo_table);
}

void
routing_deflection (struct node *node, struct routing *routing)
{
  struct deflection_info **dinfo_table = routing->data;
  int s, t;
  struct vector_node *vn, *vnn, *vne;
  struct link *link;
  struct node *neighbor;
  struct prev_hop_entry *entry;
  struct graph *G1, *G2;
  struct node *neighbor1, *node2;
  struct candidate **table1, **table2;
  struct link *link1, *link2;
  int i;

  s = node->id;

  /* For each given destination */
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *dst = (struct node *) vn->data;
      t = dst->id;

      fprintf (stderr, "  Calculation deflection for destionation %d ... \n", t);
      fflush (stderr);

      if (! dinfo_table[node->id]->table[t])
        {
          fprintf (stderr, "    ignoring, no default route to t, "
                   "maybe it is disconnected (other connected component)\n");
          continue;
        }

      /* initialize */
      dinfo_table[node->id]->deflection_set_1[t] = vector_create ();
      dinfo_table[node->id]->prev_hop_table[t] = vector_create ();
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          link = (struct link *) vnn->data;
          neighbor = link->to;

          entry = prev_hop_entry_create ();
          entry->prev_hop = neighbor;
          entry->deflection_set_2 = vector_create ();
          entry->deflection_set_3 = vector_create ();
          vector_add (entry, dinfo_table[node->id]->prev_hop_table[t]);
        }

      /* rule 1 (One Hop Down) */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          link = (struct link *) vnn->data;
          neighbor = link->to;

          if (dinfo_table[neighbor->id]->table[t]->metric <
              dinfo_table[node->id]->table[t]->metric)
            vector_add (neighbor, dinfo_table[node->id]->deflection_set_1[t]);
        }

      /* rule 2 (Two Hops Down) */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          link = (struct link *) vnn->data;
          neighbor = link->to;

          for (vne = vector_head (dinfo_table[node->id]->prev_hop_table[t]); vne;
               vne = vector_next (vne))
            {
              entry = (struct prev_hop_entry *) vne->data;

              if (dinfo_table[neighbor->id]->table[t]->metric <
                  dinfo_table[node->id]->table[t]->metric)
                vector_add (neighbor, entry->deflection_set_2);
              else if (dinfo_table[neighbor->id]->table[t]->metric <
                       dinfo_table[entry->prev_hop->id]->table[t]->metric)
                vector_add (neighbor, entry->deflection_set_2);
            }
        }

      /* rule 3 (Two Hops Forward) */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          link = (struct link *) vnn->data;
          neighbor = link->to;

          for (vne = vector_head (dinfo_table[node->id]->prev_hop_table[t]); vne;
               vne = vector_next (vne))
            {
              entry = (struct prev_hop_entry *) vne->data;

              if (neighbor == entry->prev_hop)
                continue;

              G1 = graph_copy_create (node->g);
              neighbor1 = node_lookup (neighbor->id, G1);
              link1 = link_lookup_by_node_id (node->id, neighbor->id, G1);
              link_delete (link1);
              link1 = link_lookup_by_node_id (neighbor->id, node->id, G1);
              link_delete (link1);
              table1 = (struct candidate **)
                calloc (G1->nodes->size, sizeof (struct candidate *));
              routing_dijkstra (neighbor1, routing->W, table1, NULL);

              G2 = graph_copy_create (node->g);
              node2 = node_lookup (node->id, G2);
#if 1 /* if 0, Rule-3 would have been a subset of Rule-2 ... */
              link2 = link_lookup_by_node_id (entry->prev_hop->id, node->id, G2);
              link_delete (link2);
              link2 = link_lookup_by_node_id (node->id, entry->prev_hop->id, G2);
              link_delete (link2);
#endif
              table2 = (struct candidate **)
                calloc (G2->nodes->size, sizeof (struct candidate *));
              routing_dijkstra (node2, routing->W, table2, NULL);

#if 0
              fprintf (stderr, "Rule3: l_{i+1}: %d-%d, l_{i}: %d-%d, "
                       "n_{i+1}: %d, n_{i}: %d, n_{i-1}: %d\n",
                       node->id, neighbor->id,
                       entry->prev_hop->id, node->id,
                       neighbor->id, node->id, entry->prev_hop->id);
#endif

              if (! table1[t] || ! table2[t])
                {
                  fprintf (stderr, "Can't calculate Rule-3: nexthop: %d "
                           "s: %d t: %d prev: %d no route in %s\n",
                           neighbor->id, s, t, entry->prev_hop->id,
                           (! table1[t] ? "G1" : "G2"));

                  graph_delete (G1);
                  for (i = 0; i < routing->nnodes; i++)
                    if (table1[i])
                      candidate_delete (table1[i]);
                  free (table1);

                  graph_delete (G2);
                  for (i = 0; i < routing->nnodes; i++)
                    if (table2[i])
                      candidate_delete (table2[i]);
                  free (table2);

                  continue;
                }

              if (table1[t]->metric < table2[t]->metric)
                {
#if 0
                  fprintf (stderr, "rule3: downhill: prev: %d node: %d nei: %d\n",
                           entry->prev_hop->id, node->id, neighbor->id);
                  fprintf (stderr, "       c{G-{%u,%u},%u}:[",
                           neighbor->id, node->id, neighbor->id);
                  print_nodelist (stderr, (struct vector *) table1[t]->paths->array[0]);
                  fprintf (stderr, "](%lu) < c{G-{%u,%u},%u}:[",
                           table1[t]->metric,
                           entry->prev_hop->id, node->id, node->id);
                  print_nodelist (stderr, (struct vector *) table2[t]->paths->array[0]);
                  fprintf (stderr, "](%lu)\n", table1[t]->metric);
#endif

                  vector_add (neighbor, entry->deflection_set_3);
                }
              else if (table1[t]->metric <
                       dinfo_table[entry->prev_hop->id]->table[t]->metric)
                {
#if 0
                  fprintf (stderr, "rule3: two-hop: prev: %d node: %d nei: %d\n",
                           entry->prev_hop->id, node->id, neighbor->id);
                  fprintf (stderr, "       c{G-{%u,%u},%u}:[",
                           neighbor->id, node->id, neighbor->id);
                  print_nodelist (stderr, (struct vector *) table1[t]->paths->array[0]);
                  fprintf (stderr, "](%lu) < c{G,%u}:[",
                           table1[t]->metric, entry->prev_hop->id);
                  print_nodelist (stderr, (struct vector *)
                    dinfo_table[entry->prev_hop->id]->table[t]->paths->array[0]);
                  fprintf (stderr, "](%lu)\n", 
                           dinfo_table[entry->prev_hop->id]->table[t]->metric);
#endif

                  vector_add (neighbor, entry->deflection_set_3);
                }

              graph_delete (G1);
              for (i = 0; i < routing->nnodes; i++)
                if (table1[i])
                  candidate_delete (table1[i]);
              free (table1);

              graph_delete (G2);
              for (i = 0; i < routing->nnodes; i++)
                if (table2[i])
                  candidate_delete (table2[i]);
              free (table2);

            }
        }

    }
}

DEFINE_COMMAND (routing_algorithm_deflection,
                "routing-algorithm deflection",
                "calculate routing using algorithm\n"
                "Deflection routing based on SPF.\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;

  struct vector_node *vn;
  struct node *node;
  struct deflection_info **dinfo_table;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }
  if (routing->W == NULL)
    {
      fprintf (shell->terminal, "no weight specified for routing.\n");
      return;
    }
  if (routing->G != routing->W->G)
    {
      fprintf (shell->terminal, "base graph does not match with weight's.\n");
      return;
    }

  dinfo_table = (struct deflection_info **)
    calloc (routing->nnodes, sizeof (struct dinfo_table *));

  if (routing->data)
    dinfo_table_delete ((struct deflection_info **) routing->data);
  routing->data = dinfo_table;
  routing->data_free = dinfo_table_delete;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      dinfo_table[node->id] = deflection_info_create ();
      dinfo_table[node->id]->nnodes = routing->nnodes;
      dinfo_table[node->id]->table = (struct candidate **)
        calloc (routing->nnodes, sizeof (struct candidate *));

      dinfo_table[node->id]->deflection_set_1 = (struct vector **)
        calloc (routing->nnodes, sizeof (struct vector *));
      dinfo_table[node->id]->prev_hop_table = (struct vector **)
        calloc (routing->nnodes, sizeof (struct vector *));

      routing_dijkstra (node, routing->W, dinfo_table[node->id]->table, NULL);
      routing_dijkstra_route (node, dinfo_table[node->id]->table, routing);
    }

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      fprintf (stderr, "Calculating deflection set for node[%d]:\n", node->id);
      fflush (stderr);

      routing_deflection (node, routing);
    }
}


DEFINE_COMMAND (show_deflection_set,
                "show deflection set",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "display deflection neighbor set\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vn, *vnn, *vne;
  struct node *node, *dst;
  int t;
  struct prev_hop_entry *entry;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          if (! dinfo_table[node->id]->table[t])
            {
              fprintf (stderr, "    ignoring, no default route to t, "
                       "maybe it is disconnected "
                       "(other connected component)\n");
              continue;
            }

          /* Print */
          fprintf (shell->terminal, "  node[%d] dest: %d Default: ", node->id, t);
          print_nodelist (shell->terminal, dinfo_table[node->id]->table[t]->nexthops);
          fprintf (shell->terminal, "\n");
          fprintf (shell->terminal, "  node[%d] dest: %d Deflection-1: ", node->id, t);
          print_nodelist (shell->terminal, dinfo_table[node->id]->deflection_set_1[t]);
          fprintf (shell->terminal, "\n");

          for (vne = vector_head (dinfo_table[node->id]->prev_hop_table[t]); vne;
               vne = vector_next (vne))
            {
              entry = (struct prev_hop_entry *) vne->data;
              fprintf (shell->terminal, "  node[%d] dest: %d incoming: %d: ",
                       node->id, t, entry->prev_hop->id);
              fprintf (shell->terminal, "Deflection-2: ");
              print_nodelist (shell->terminal, entry->deflection_set_2);
              fprintf (shell->terminal, "\n");
    
              fprintf (shell->terminal, "  node[%d] dest: %d incoming: %d: ",
                       node->id, t, entry->prev_hop->id);
              fprintf (shell->terminal, "Deflection-3: ");
              print_nodelist (shell->terminal, entry->deflection_set_3);
              fprintf (shell->terminal, "\n");
            }
          fflush (shell->terminal);
        }

    }
}

DEFINE_COMMAND (save_deflection_set,
                "save deflection-set file <FILENAME>",
                "save information\n"
                "Deflection routing based on SPF.\n"
                "save deflection-set to file\n"
                "Specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vni, *vnj, *vnk, *vnl;
  struct node *node, *dst;
  int s, t;
  struct prev_hop_entry *entry;
  struct node *x;
  FILE *fp;

  fp = fopen (argv[3], "w+");
  if (! fp)
    {
      fprintf (shell->terminal, "cant open file: %s\n", argv[3]);
      return;
    }

  for (vni = vector_head (routing->G->nodes); vni; vni = vector_next (vni))
    {
      node = (struct node *) vni->data;
      s = node->id;

      for (vnj = vector_head (routing->G->nodes); vnj; vnj = vector_next (vnj))
        {
          dst = (struct node *) vnj->data;
          t = dst->id;

          if (! dinfo_table[node->id]->table[t])
            {
              fprintf (stderr, "    ignoring, no default route to t, "
                       "maybe it is disconnected "
                       "(other connected component)\n");
              continue;
            }

          for (vnk = vector_head (dinfo_table[s]->table[t]->nexthops); vnk;
               vnk = vector_next (vnk))
            {
              x = (struct node *) vnk->data;
              fprintf (fp, "set deflection-set node %d destination %d "
                       "default nexthop %d\n", s, t, x->id);
            }

          for (vnk = vector_head (dinfo_table[s]->deflection_set_1[t]); vnk;
               vnk = vector_next (vnk))
            {
              x = (struct node *) vnk->data;
              fprintf (fp, "set deflection-set node %d destination %d "
                       "rule-1 nexthop %d\n", s, t, x->id);
            }

          for (vnk = vector_head (dinfo_table[s]->prev_hop_table[t]); vnk;
               vnk = vector_next (vnk))
            {
              entry = (struct prev_hop_entry *) vnk->data;

              for (vnl = vector_head (entry->deflection_set_2); vnl;
                   vnl = vector_next (vnl))
                {
                  x = (struct node *) vnl->data;
                  fprintf (fp, "set deflection-set node %d destination %d "
                           "rule-2 prev-hop %d nexthop %d\n",
                           s, t, entry->prev_hop->id, x->id);
                }

              for (vnl = vector_head (entry->deflection_set_3); vnl;
                   vnl = vector_next (vnl))
                {
                  x = (struct node *) vnl->data;
                  fprintf (fp, "set deflection-set node %d destination %d "
                           "rule-3 prev-hop %d nexthop %d\n",
                           s, t, entry->prev_hop->id, x->id);
                }

            }
        }
    }

  fflush (fp);
  fclose (fp);
}

#define REGMATCH_MAX 16

#define START "^"
#define END "$"
#define SPACE "[ \t]+"
#define MAYSPACE "[ \t]*"

/* set deflection-set node 0 destination 0 default nexthop 0 */

#define DEFLECTION_LINE \
  "^set deflection-set node ([0-9]+) destination ([0-9]+) " \
  "([a-zA-Z0-9-]+) " "([a-zA-Z0-9-]*)" MAYSPACE "([0-9]*)" MAYSPACE \
  "nexthop ([0-9]+)" MAYSPACE END

DEFINE_COMMAND (load_deflection_set,
                "load deflection-set file <FILENAME>",
                "load information\n"
                "Deflection routing based on SPF.\n"
                "load deflection-set to file\n"
                "Specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;
  int rargc;
  char *rargv[REGMATCH_MAX];
  regex_t regex;
  regmatch_t regmatch[REGMATCH_MAX];

  char buf[1024];
  int i, ret;

  struct vector_node *vni, *vnj, *vnk;
  struct node *s, *t;
  struct prev_hop_entry *entry, *match;

  int s_id, t_id, prev_hop_id, nexthop_id;
  char *type;
  struct node *prev_hop, *nexthop;
  struct link *link;

  FILE *fp;
  struct node *neighbor;
  char *line;
  char *endptr;

  fp = fopen (argv[3], "r");
  if (! fp)
    {
      fprintf (shell->terminal, "cant open file: %s\n", argv[3]);
      return;
    }

  dinfo_table = (struct deflection_info **)
    calloc (routing->nnodes, sizeof (struct dinfo_table *));

  if (routing->data)
    dinfo_table_delete ((struct deflection_info **) routing->data);
  routing->data = dinfo_table;
  routing->data_free = dinfo_table_delete;

  for (vni = vector_head (routing->G->nodes); vni;
       vni = vector_next (vni))
    {
      s = (struct node *) vni->data;

      /* initialize */
      dinfo_table[s->id] = deflection_info_create ();
      dinfo_table[s->id]->nnodes = routing->nnodes;
      dinfo_table[s->id]->table = (struct candidate **)
        calloc (routing->nnodes, sizeof (struct candidate *));

      dinfo_table[s->id]->deflection_set_1 = (struct vector **)
        calloc (routing->nnodes, sizeof (struct vector *));
      dinfo_table[s->id]->prev_hop_table = (struct vector **)
        calloc (routing->nnodes, sizeof (struct vector *));

      for (vnj = vector_head (routing->G->nodes); vnj;
           vnj = vector_next (vnj))
        {
          t = (struct node *) vnj->data;

          /* initialize */
          dinfo_table[s->id]->deflection_set_1[t->id] = vector_create ();
          dinfo_table[s->id]->prev_hop_table[t->id] = vector_create ();
          for (vnk = vector_head (s->olinks); vnk; vnk = vector_next (vnk))
            {
              link = (struct link *) vnk->data;
              neighbor = link->to;

              entry = prev_hop_entry_create ();
              entry->prev_hop = neighbor;
              entry->deflection_set_2 = vector_create ();
              entry->deflection_set_3 = vector_create ();
              vector_add (entry, dinfo_table[s->id]->prev_hop_table[t->id]);
            }
        }
    }

  ret = regcomp (&regex, DEFLECTION_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp failed: %s: %s\n", buf,
               DEFLECTION_LINE);
      return;
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      if (strlen (buf) == sizeof (buf) - 1)
        fprintf (shell->terminal, "*** buffer too short ***\n");

      ret = regexec (&regex, buf, REGMATCH_MAX, regmatch, 0);
      if (ret == REG_NOMATCH)
        {
          fprintf (shell->terminal, "%s", DEFLECTION_LINE);
          fprintf (shell->terminal, "match failed: %s", buf);
          return;
        }
#if 0
      else
        fprintf (shell->terminal, "matched: %s", buf);
#endif

      line = strdup (buf);
      rargc = 0;

      /* regmatch[0] is the entire strings that matched */
      for (i = 1; i < REGMATCH_MAX; i++)
        {
          if (regmatch[i].rm_so == -1)
            rargv[i-1] = NULL;
          else
            {
              line[regmatch[i].rm_eo] = '\0';
              rargv[i-1] = &line[regmatch[i].rm_so];
              rargc = i;
            }
        }

      s_id = strtol (rargv[0], &endptr, 0);
      if (*endptr != '\0')
        {
          fprintf (shell->terminal, "invalid node id: %s\n", rargv[0]);
          return;
        }

      t_id = strtol (rargv[1], &endptr, 0);
      if (*endptr != '\0')
        {
          fprintf (shell->terminal, "invalid node id: %s\n", rargv[1]);
          return;
        }

      type = strdup (rargv[2]);
      if (type == NULL)
        {
          fprintf (shell->terminal, "strdup for type failed: %s\n",
                   strerror (errno));
          return;
        }

      if (rargv[3] && strlen (rargv[3]))
        {
          assert (! strcmp (rargv[3], "prev-hop"));
          prev_hop_id = strtol (rargv[4], &endptr, 0);
          if (*endptr != '\0')
            {
              fprintf (shell->terminal, "invalid node id: %s\n", rargv[4]);
              return;
            }
        }

      nexthop_id = strtol (rargv[5], &endptr, 0);
      if (*endptr != '\0')
        {
          fprintf (shell->terminal, "invalid node id: %s\n", rargv[5]);
          return;
        }

      s = node_get (s_id, routing->G);
      t = node_get (t_id, routing->G);
      nexthop = node_get (nexthop_id, routing->G);

      if (! strcmp (type, "default"))
        {
          if (! dinfo_table[s->id]->table[t->id])
            {
              struct candidate *c = candidate_create ();
              c->destination = t;
              vector_add (nexthop, c->nexthops);
              dinfo_table[s->id]->table[t->id] = c;
            }
          else
            {
              struct candidate *c = dinfo_table[s->id]->table[t->id];
              assert (c->destination == t);
              vector_add (nexthop, c->nexthops);
            }
        }
      else if (! strcmp (type, "rule-1"))
        {
          vector_add (nexthop, dinfo_table[s->id]->deflection_set_1[t->id]);
        }
      else if (! strcmp (type, "rule-2"))
        {
          prev_hop = node_get (prev_hop_id, routing->G);

          match = NULL;
          for (vni = vector_head (dinfo_table[s->id]->prev_hop_table[t->id]);
               vni; vni = vector_next (vni))
            {
              entry = (struct prev_hop_entry *) vni->data;
              if (entry->prev_hop == prev_hop)
                match = entry;
            }
          if (! match)
            assert (0);

          vector_add (nexthop, match->deflection_set_2);
        }
      else if (! strcmp (type, "rule-3"))
        {
          prev_hop = node_get (prev_hop_id, routing->G);

          match = NULL;
          for (vni = vector_head (dinfo_table[s->id]->prev_hop_table[t->id]);
               vni; vni = vector_next (vni))
            {
              entry = (struct prev_hop_entry *) vni->data;
              if (entry->prev_hop == prev_hop)
                match = entry;
            }
          if (! match)
            assert (0);

          vector_add (nexthop, match->deflection_set_3);
        }

      free (line);
    }

  for (vni = vector_head (routing->G->nodes); vni; vni = vector_next (vni))
    {
      s = (struct node *) vni->data;
      routing_dijkstra_route (s, dinfo_table[s->id]->table, routing);
    }
}


DEFINE_COMMAND (show_deflection_number,
                "show deflection number",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "display the number of deflection neighbor in the set\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vn, *vnn, *vne;
  struct node *node, *dst;
  int s, t, rx;
  unsigned int nnodes = routing->G->nodes->size;
  struct prev_hop_entry *entry;

  unsigned int default_count = 0;
  unsigned int d1_count = 0;
  unsigned int d2_count = 0;
  unsigned int d3_count = 0;
  unsigned int incomings = 0;
  unsigned int st_pairs = 0;

  fprintf (shell->terminal, "EVAL: %2s-%2s %2s: %4s %4s %4s %4s\n",
           "ss", "tt", "rx", "def", "D-1", "D-2", "D-3");

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      s = node->id;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          /* do not count from dst to dst */
          if (node == dst)
            continue;

          if (! dinfo_table[node->id]->table[t])
            {
#if 0
              fprintf (stderr, "    ignoring, no default route to t, "
                       "maybe it is disconnected "
                       "(other connected component)\n");
#endif
              continue;
            }

          st_pairs++;

          default_count += dinfo_table[node->id]->table[t]->nexthops->size;
          d1_count += dinfo_table[node->id]->deflection_set_1[t]->size;

          for (vne = vector_head (dinfo_table[node->id]->prev_hop_table[t]); vne;
               vne = vector_next (vne))
            {
              entry = (struct prev_hop_entry *) vne->data;
              rx = entry->prev_hop->id;

              /* do not count incoming from dst */
              if (entry->prev_hop == dst)
                continue;

              incomings++;

              d2_count += entry->deflection_set_2->size;
              d3_count += entry->deflection_set_3->size;

              fprintf (shell->terminal, "EVAL: %02d-%02d %2d: %4d %4d %4d %4d\n",
                       s, t, rx,
                       dinfo_table[node->id]->table[t]->nexthops->size,
                       dinfo_table[node->id]->deflection_set_1[t]->size,
                       entry->deflection_set_2->size,
                       entry->deflection_set_3->size);
            }
        }
    }

  fprintf (shell->terminal, "%6s %8s %8s %8s %8s %8s %8s\n",
           "#Nodes", "#s-t", "#def", "#D1", "#inc", "#D2", "#D3");
  fprintf (shell->terminal, "%6d %8u %8u %8u %8u %8u %8u\n",
           nnodes, st_pairs, default_count, d1_count,
           incomings, d2_count, d3_count);
  fprintf (shell->terminal, "%6s %8s %8.2f %8.2f %8.2f %8.2f %8.2f\n",
           "Average", "",
           (double) default_count / st_pairs, (double) d1_count / st_pairs,
           (double) incomings / st_pairs, (double) d2_count / incomings,
           (double) d3_count / incomings);
}

void
deflect_forward (struct path *path, struct node *dst,
                 struct deflection_info **dinfo_table, int rule)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct prev_hop_entry *e, *entry;
  struct vector *deflection_set;
  struct vector_node *vn;

  assert (rule == 1 || rule == 2 || rule == 3);

  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

#if 0
      fprintf (stderr, "  before: path: ");
      print_nodelist (stderr, path->path);
      fprintf (stderr, "\n");
      fflush (stderr);
#endif

      if (rule == 1 || ! prev)
        {
          deflection_set = dinfo_table[current->id]->deflection_set_1[dst->id];
        }
      else
        {
          entry = NULL;
          for (vn = vector_head (dinfo_table[current->id]->prev_hop_table[dst->id]);
               vn; vn = vector_next (vn))
            {
              e = (struct prev_hop_entry *) vn->data;
              if (e->prev_hop == prev)
                entry = e;
            }
          assert (entry);

          if (rule == 2)
            deflection_set = entry->deflection_set_2;
          else if (rule == 3)
            deflection_set = entry->deflection_set_3;
        }

      assert (deflection_set && deflection_set->size);
      next = vector_get (deflection_set, 0);
      vector_add_allow_dup (next, path->path);
    }
}

struct path *
deflect_branch (struct path *path, struct node *dst,
                struct deflection_info **dinfo_table, int rule)
{
  struct node *prev, *current, *next, *prevprev = NULL;
  int index;
  struct vector_node *vn;
  struct vector *deflection_set;
  struct prev_hop_entry *e, *entry;

  assert (rule == 1 || rule == 2 || rule == 3);

  /* for each branch level */
  for (index = path->path->size - 1; index > 0; index--)
    {
      prev = vector_get (path->path, index - 1);
      current = vector_get (path->path, index);
      if (index > 1)
        prevprev = vector_get (path->path, index - 2);
      else
        prevprev = NULL;

#if 0
      fprintf (stderr, "  before: path: ");
      print_nodelist (stderr, path->path);
      fprintf (stderr, "\n");
      fflush (stderr);
#endif

      if (rule == 1 || ! prevprev)
        {
          deflection_set = dinfo_table[prev->id]->deflection_set_1[dst->id];
        }
      else
        {
          entry = NULL;
          for (vn = vector_head (dinfo_table[prev->id]->prev_hop_table[dst->id]);
               vn; vn = vector_next (vn))
            {
              e = (struct prev_hop_entry *) vn->data;

#if 0
              fprintf (stderr, "    prev[%d]->(prev[%d]?)node[%d]->...->dst[%d]\n",
                       prevprev->id, e->prev_hop->id, prev->id, dst->id);
              fflush (stderr);
#endif

              if (e->prev_hop == prevprev)
                entry = e;
            }
          assert (entry);

          if (rule == 2)
            deflection_set = entry->deflection_set_2;
          else if (rule == 3)
            deflection_set = entry->deflection_set_3;
        }

#if 0
      fprintf (stderr, "    node[%d]: prev[%d]: deflect-%d: ",
               current->id, prev->id, rule);
      print_nodelist (stderr, deflection_set);
      fprintf (stderr, "\n");
      fflush (stderr);
#endif

      /* find current branch and next branch of the path */
      next = NULL;
      for (vn = vector_head (deflection_set); vn; vn = vector_next (vn))
        {
          struct node *node = (struct node *) vn->data;
          if (node != current)
            continue;

          vn = vector_next (vn);
          if (! vn)
            break;

          next = (struct node *) vn->data;
          vector_break (vn);
          break;
        }

      /* change to the next branch */
      if (next)
        {
          vector_set (path->path, index, next);
          break;
        }

      /* remove already traversed part
         (and go to previous level of branching) */
      vector_remove_index (index, path->path);
    }

  /* no more branch, search end */
  if (index == 0)
    {
      path_delete (path);
      return NULL;
    }

  return path;
}



struct path *
deflect_path_first (struct node *src, struct node* dst,
                    struct deflection_info **dinfo_table, int rule)
{
  struct path *path = path_create ();

  assert (rule == 1 || rule == 2 || rule == 3);

  vector_add (src, path->path);
  deflect_forward (path, dst, dinfo_table, rule);

  return path;
}

struct path *
deflect_path_next (struct path *path, struct node* dst,
                   struct deflection_info **dinfo_table, int rule)
{
  assert (rule == 1 || rule == 2 || rule == 3);

  /* go to the next branch of the path */
  path = deflect_branch (path, dst, dinfo_table, rule);

  if (path)
    {
      /* lengthen path to the destination */
      deflect_forward (path, dst, dinfo_table, rule);
      assert (path_end (path) == dst);
    }

  return path;
}



DEFINE_COMMAND (show_deflection_path_1,
                "show deflection deflect-1 path",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "Deflection routing using Rule-1.\n"
                "display the deflection paths\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vn, *vnn;
  struct node *src, *dst;
  int s, t;
  struct path *path;
  unsigned int path_count;
  unsigned int path_size;
  unsigned int path_size_max;
  int rule = 0;

  if (! strcmp (argv[2], "deflect-1"))
    rule = 1;
  else if (! strcmp (argv[2], "deflect-2"))
    rule = 2;
  else if (! strcmp (argv[2], "deflect-3"))
    rule = 3;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      src = (struct node *) vn->data;
      s = src->id;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          /* do not count from dst to dst */
          if (src == dst)
            continue;

#if 0
          snprintf (filename, sizeof (filename),
                    "deflect-%d-paths/%d-%d.paths", rule, s, t);
          fp = fopen_create (filename, "w+");
#endif

          path_count = 0;
          path_size = 0;
          path_size_max = 0;
          for (path = deflect_path_first (src, dst, dinfo_table, rule); path;
               path = deflect_path_next (path, dst, dinfo_table, rule))
            {
#if 0
              fprintf (fp, "%d-%d deflect-%d path: ", s, t, rule);
              print_nodelist (fp, path->path);
              fprintf (fp, "\n");
#endif

              path_count++;
              path_size += path->path->size;
              if (path_size_max < path->path->size)
                path_size_max = path->path->size;
            }

          fprintf (shell->terminal,
                   "EVAL: %02d-%02d deflect-%d #paths: %u avglen: %f maxlen: %u\n",
                   s, t, rule, path_count, (double) path_size / path_count,
                   path_size_max);

#if 0
          fprintf (fp,
                   "EVAL: %02d-%02d deflect-%d #paths: %u avglen: %f maxlen: %u\n",
                   s, t, rule, path_count, (double) path_size / path_count,
                   path_size_max);
          fclose (fp);
#endif
        }
    }
}

ALIAS_COMMAND (show_deflection_path_2,
               show_deflection_path_1,
                "show deflection deflect-2 path",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "Deflection routing using Rule-2.\n"
                "display the deflection paths\n");

ALIAS_COMMAND (show_deflection_path_3,
               show_deflection_path_1,
                "show deflection deflect-3 path",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "Deflection routing using Rule-3.\n"
                "display the deflection paths\n");

DEFINE_COMMAND (show_deflection_failure_simulate,
                "show deflection failure simulate",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "Failure simulation.\n"
                "Failure simulation.\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vn, *vnn;
  struct node *src, *dst, *failure;
  int s, t;
  struct path *path;
  unsigned int path_count;
  unsigned int path_size;
  unsigned int path_size_max;
  int rule = 0;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      src = (struct node *) vn->data;
      s = src->id;

      if (failure == src)
        continue;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          if (failure == dst)
            continue;

          /* do not count from dst to dst */
          if (src == dst)
            continue;

          path_count = 0;
          path_size = 0;
          path_size_max = 0;
          for (path = deflect_path_first (src, dst, dinfo_table, rule); path;
               path = deflect_path_next (path, dst, dinfo_table, rule))
            {
#if 0
              fprintf (shell->terminal, "%d-%d deflect-%d path: ", s, t, rule);
              print_nodelist (shell->terminal, path->path);
              fprintf (shell->terminal, "\n");
              fflush (shell->terminal);
#endif

              path_count++;
              path_size += path->path->size;
              if (path_size_max < path->path->size)
                path_size_max = path->path->size;
            }

          fprintf (shell->terminal, "EVAL: %02d-%02d deflect-%d #paths: %u avglen: %f maxlen: %u\n",
                   s, t, rule, path_count, (double) path_size / path_count, path_size_max);
          fflush (shell->terminal);
        }
    }

}



DEFINE_COMMAND (show_deflection_path_number_rule_1,
                "show deflection path number rule-1",
                "display information\n"
                "Deflection routing based on SPF.\n"
                "Deflection routing paths.\n"
                "number of Deflection routing paths.\n"
                "Deflection routing using Rule-1.\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct deflection_info **dinfo_table = routing->data;

  struct vector_node *vn, *vnn;
  struct node *src, *dst;
  int s, t;
  struct path *path;
  unsigned int path_count;
  unsigned int path_size;
  unsigned int path_size_max;
  int rule = 0;

  time_t seed;
  int prime;
  struct vector *prime_vector;
  struct vector *router_primes;
  struct table *path_table;
  struct node *node;
  int i;
  int tag;

  char pathstring[2048];

  seed = time (NULL);
  fprintf (shell->terminal, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* create router seeds (prime value) */
  prime_vector = prime_vector_create (120);
#if 1
  fprintf (stderr, "prime vector: ");
  for (i = 0; i < prime_vector->size; i++)
    fprintf (stderr, " %d", (int) vector_get (prime_vector, i));
  fprintf (stderr, "\n");
#endif /*1*/

  router_primes = vector_create ();
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      prime = (int) vector_get (prime_vector, random () % 10 + 10);
      vector_set (router_primes, node->id, (void *) prime);
    }
  vector_delete (prime_vector);

  if (! strcmp (argv[4], "rule-1"))
    rule = 1;
  else if (! strcmp (argv[4], "rule-2"))
    rule = 2;
  else if (! strcmp (argv[4], "rule-3"))
    rule = 3;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      src = (struct node *) vn->data;
      s = src->id;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          /* do not count from dst to dst */
          if (src == dst)
            continue;

          /* ignore disconnected nodes */
          if (routing->route[s][t].nexthops->size == 0)
            {
              fprintf (shell->terminal, "no route %d-%d, maybe disconnected\n",
                       s, t);
              continue;
            }

          path_table = table_create ();

          path_count = 0;
          path_size = 0;
          path_size_max = 0;

          for (tag = 0; tag < 1024; tag++)
            {
              path = path_create ();
              vector_add (src, path->path);

              if (rule == 1)
                deflect_1_tag_forward (dst, tag, dinfo_table,
                  deflection_selection, router_primes, path);
              else if (rule == 2)
                deflect_2_tag_forward (dst, tag, dinfo_table,
                  deflection_selection, router_primes, path);
              else if (rule == 3)
                deflect_3_tag_forward (dst, tag, dinfo_table,
                  deflection_selection, router_primes, path);
              else
                assert (0);

              sprint_nodelist (pathstring, sizeof (pathstring), path->path);
              assert (strlen (pathstring) < sizeof (pathstring) - 1);

#if 0
              fprintf (shell->terminal, "deflect-%d %02d-%02d tag: %d path: ",
                       rule, s, t, tag);
              fprintf (shell->terminal, "%s\n", pathstring);
              fflush (shell->terminal);
#endif

              if (! table_lookup (pathstring, strlen (pathstring) * 8,
                                  path_table))
                {
                  path_count++;
                  path_size += path->path->size;
                  if (path_size_max < path->path->size)
                    path_size_max = path->path->size;

                  table_add (pathstring, strlen (pathstring) * 8,
                             (void *) "exist", path_table);
                }

              path_delete (path);
            }

          table_delete (path_table);

          fprintf (shell->terminal,
                   "EVAL: deflect-%d %02d-%02d #paths: %u avglen: %f maxlen: %u\n",
                   rule, s, t, path_count, (double) path_size / path_count,
                   path_size_max);

        }
    }
}

ALIAS_COMMAND (show_deflection_path_number_rule_2,
               show_deflection_path_number_rule_1,
               "show deflection path number rule-2",
               "display information\n"
               "Deflection routing based on SPF.\n"
               "Deflection routing paths.\n"
               "number of Deflection routing paths.\n"
               "Deflection routing using Rule-1.\n");

ALIAS_COMMAND (show_deflection_path_number_rule_3,
               show_deflection_path_number_rule_1,
               "show deflection path number rule-3",
               "display information\n"
               "Deflection routing based on SPF.\n"
               "Deflection routing paths.\n"
               "number of Deflection routing paths.\n"
               "Deflection routing using Rule-1.\n");


