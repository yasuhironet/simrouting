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
#include "shell.h"
#include "command.h"
#include "file.h"

#include "network/graph.h"
#include "network/weight.h"

#ifdef HAVE_GRAPHVIZ

#include <gvc.h>

Agraph_t *GVG;
struct vector *agnodes;
struct vector *agedges;

char *
get_subdomain (char *name, char *parent_domain)
{
  char *dp, *sp;
  int count = 0;

  if (! (dp = strstr (name, parent_domain)))
    return NULL;

  for (sp = dp - 1; name < sp; sp--)
    {
      if (*sp != '.')
        count ++;
      if (count && *sp == '.')
        break;
    }

  if (*sp == '.')
    sp++;
  else
    sp = NULL;

  return sp;
}

Agraph_t *
get_cluster (char *subdomain, Agraph_t *GVG)
{
  char label[256];
  Agraph_t *g = NULL;

  if (! subdomain)
    return NULL;

  snprintf (label, sizeof (label), "cluster %s", subdomain);
  g = agsubg (GVG, label);
  agset (g, "clusterrank", "local");
  agset (g, "label", subdomain);

  return g;
}

Agnode_t *
simrouting_agnode (struct node *node, Agraph_t *GVG, char *domain)
{
  Agraph_t *g = GVG;
  Agnode_t *agn;
  char simple_name[64];

  snprintf (simple_name, sizeof (simple_name), "%d", node->id);
  if (! node->name)
    node->name = strdup (simple_name);

  if (domain && node->domain_name)
    {
      char *subdomain = get_subdomain (node->domain_name, domain);
      g = get_cluster (subdomain, GVG);
      if (g == NULL)
        g = GVG;
    }

  agn = agnode (g, node->name);
  agset (agn, "shape", "point");

  /* node position */
  if (node->xpos || node->ypos)
    {
      char position[64];
      snprintf (position, sizeof (position), "%d,%d",
                (int)node->xpos, (int)node->ypos);
      agset (agn, "pos", position);
    }

  /* node color */
  if (node->type == NODE_TYPE_ROUTER)
    agset (agn, "color", "black");
  else if (node->type == NODE_TYPE_NETWORK)
    agset (agn, "color", "blue");

  return agn;
}

void
graphviz_init (struct shell *shell, struct graph *G,
               Agraph_t **GVG, struct vector **agnodes, struct vector **agedges)
{
  *agnodes = vector_create ();
  *agedges = vector_create ();
  aginit ();
  *GVG = agopen (G->name, AGDIGRAPH);
}

void
graphviz_export (struct shell *shell, struct graph *G,
                 Agraph_t *GVG, struct vector *agnodes, struct vector *agedges,
                 char *domain)
{
  struct vector_node *i, *j;
  struct node *node, *peer;
  struct link *link;
  Agnode_t *agn, *agp;
  Agedge_t *age;

  agraphattr (GVG, "clusterrank", "none");
  agraphattr (GVG, "label", "");
  agnodeattr (GVG, "color", "black");
  agnodeattr (GVG, "shape", "point");
  agnodeattr (GVG, "pos", "0,0");
  agnodeattr (GVG, "label", "");
  agedgeattr (GVG, "dir", "forward");
  //agedgeattr (GVG, "dir", "none");
  agedgeattr (GVG, "arrowhead", "none");
  agedgeattr (GVG, "arrowtail", "none");
  agedgeattr (GVG, "color", "black");
  agedgeattr (GVG, "label", "");
  agedgeattr (GVG, "headlabel", "");
  agedgeattr (GVG, "taillabel", "");

  for (i = vector_head (G->nodes); i; i = vector_next (i))
    {
      node = (struct node *) i->data;
      if (! node)
        continue;

      agn = simrouting_agnode (node, GVG, domain);
      vector_set (agnodes, node->id, agn);
      if (shell->terminal)
        fprintf (shell->terminal, "export node %d\n", node->id);

      for (j = vector_head (node->olinks); j; j = vector_next (j))
        {
          link = (struct link *) j->data;
          assert (link->from == node);

          if (shell->terminal)
            {
              fprintf (shell->terminal, "export link %d: %d-%d",
                       link->id, link->from->id, link->to->id);
              if (link->inverse)
                fprintf (shell->terminal, " (inv:(%d) %d-%d)",
                       link->inverse->id, link->inverse->from->id,
                       link->inverse->to->id);
              fprintf (shell->terminal, "\n");
              fflush (shell->terminal);
            }

          age = vector_get (agedges, link->id);
          if (age)
            {
              if (shell->terminal)
                fprintf (shell->terminal, "ignore link %d: already exported\n", link->id);
              continue;
            }

          peer = link->to;
          agp = vector_get (agnodes, peer->id);
          if (! agp)
            {
              agp = simrouting_agnode (peer, GVG, domain);
              vector_set (agnodes, peer->id, agp);
              if (shell->terminal)
                fprintf (shell->terminal, "export node %d\n", node->id);
            }
          assert (agp);

          age = agedge (GVG, agn, agp);
          vector_set (agedges, link->id, age);
#if 0
          if (link->inverse)
            {
              vector_set (agedges, link->inverse->id, age);
#if 0
              snprintf (name, sizeof (name), "%u", link->id);
              agset (age, "taillabel", name);
              snprintf (name, sizeof (name), "%u", link->inverse->id);
              agset (age, "headlabel", name);
#endif /*0*/
            }
          else
            {
              agset (age, "dir", "forward");
              agset (age, "arrowhead", "normal");
              agset (age, "arrowtail", "none");
            }
#endif /*0*/
        }
    }
}

void
graphviz_export_weight (struct shell *shell, struct weight *W, Agraph_t *GVG,
                        struct vector *agnodes, struct vector *agedges)
{
  struct graph *G = W->G;
  struct vector_node *vn;
  Agedge_t *age;
  char label[64];

  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      struct link *e = (struct link *) vn->data;

      age = vector_get (agedges, e->id);
      assert (age);
      snprintf (label, sizeof (label), "%lu", W->weight[e->id]);
      agset (age, "headlabel", label);

      assert (e->inverse);
      assert (age == vector_get (agedges, e->inverse->id));
      snprintf (label, sizeof (label), "%lu", W->weight[e->inverse->id]);
      agset (age, "taillabel", label);
    }
}

void
graphviz_finish (Agraph_t **GVG, struct vector **agnodes, struct vector **agedges)
{
  agclose (*GVG);
  vector_delete (*agnodes);
  vector_delete (*agedges);
}

int
graphviz_layout (Agraph_t *GVG, char *method)
{
  GVC_t *gvc;
  gvc = gvContext ();
  gvLayout (gvc, GVG, method);
  return 0;
}

int
graphviz_render (struct shell *shell, struct graph *G,
                 Agraph_t *GVG, char *method, char *format)
{
  GVC_t *gvc;
  gvc = gvContext ();
  FILE *fp = NULL;
  char filename[64];

  snprintf (filename, sizeof (filename), "%s-%s.%s",
            G->name, method, format);
  fp = fopen (filename, "w+");
  if (fp == NULL)
    {
      if (shell->terminal)
        fprintf (shell->terminal, "Can't open output file %s: %s\n",
                 filename, strerror (errno));
      return -1;
    }

  gvRender (gvc, GVG, format, fp);
  fclose (fp);

  return 0;
}

int
write_graphviz_file (struct shell *shell, struct graph *G,
                     Agraph_t *GVG, char *file)
{
  FILE *fp = NULL;

  fp = fopen_create (file, "w+");
  if (fp == NULL)
    {
      if (shell->terminal)
        fprintf (shell->terminal, "Can't open output file %s: %s\n",
                 file, strerror (errno));
      return -1;
    }

  agwrite (GVG, fp);
  fclose (fp);

  return 0;
}

DEFINE_COMMAND (export_graphviz,
                "export graphviz <FILENAME>",
                "export to other data\n"
                "export to GraphViz\n"
                "specify GraphViz filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;

  graphviz_init (shell, G, &GVG, &agnodes, &agedges);
  graphviz_export (shell, G, GVG, agnodes, agedges, NULL);
  write_graphviz_file (shell, G, GVG, argv[2]);
  graphviz_finish (&GVG, &agnodes, &agedges);
}

DEFINE_COMMAND (export_graphviz_with_weight_domain,
                "export graphviz <FILENAME> domain DOMAIN",
                "export to other data\n"
                "export to GraphViz\n"
                "specify GraphViz filename\n"
                "specify domain name to cluster subdomains\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *W = (struct weight *) shell->context;
  struct graph *G = (struct graph *) W->G;

  graphviz_init (shell, G, &GVG, &agnodes, &agedges);
  if (argc > 4)
    graphviz_export (shell, G, GVG, agnodes, agedges, argv[4]);
  else
    graphviz_export (shell, G, GVG, agnodes, agedges, NULL);
  graphviz_export_weight (shell, W, GVG, agnodes, agedges);
  write_graphviz_file (shell, G, GVG, argv[2]);
  graphviz_finish (&GVG, &agnodes, &agedges);
}

ALIAS_COMMAND (export_graphviz_with_weight,
               export_graphviz_with_weight_domain,
                "export graphviz <FILENAME>",
                "export to other data\n"
                "export to GraphViz\n"
                "specify GraphViz filename\n"
                "specify domain name to cluster subdomains\n")

DEFINE_COMMAND (export_graphviz_layout,
                "export graphviz layout fdp <FILENAME>",
                "export to other data\n"
                "export to GraphViz\n"
                "specify GraphViz filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;

  graphviz_init (shell, G, &GVG, &agnodes, &agedges);
  graphviz_export (shell, G, GVG, agnodes, agedges, NULL);
  graphviz_layout (GVG, argv[3]);
  write_graphviz_file (shell, G, GVG, argv[4]);
  graphviz_finish (&GVG, &agnodes, &agedges);
}

Agraph_t *
read_graphviz_file (struct shell *shell, char *file)
{
  FILE *fp = NULL;
  Agraph_t *GVG;

  fp = fopen (file, "r");
  if (fp == NULL)
    {
      if (shell->terminal)
        fprintf (shell->terminal, "Can't open file %s: %s\n",
                 file, strerror (errno));
      return NULL;
    }

  GVG = agread (fp);
  fclose (fp);

  return GVG;
}

void
node_import_position (struct node *node, Agnode_t *n)
{
  Agsym_t *pos;
  char *posstring, *p;
  double xpos, ypos;
  char *endptr;

  pos = agfindattr (n, "pos");
  if (! pos)
    return;

  posstring = strdup (pos->value);
  if (! posstring)
    return;

  p = strchr (posstring, ',');
  if (! p)
    {
      free (posstring);
      return;
    }

  *p++ = '\0';
  xpos = strtod (posstring, &endptr);
  if (*endptr != '\0')
    {
      free (posstring);
      return;
    }

  ypos = strtod (p, &endptr);
  if (*endptr != '\0')
    {
      free (posstring);
      return;
    }

  node->xpos = xpos;
  node->ypos = ypos;
}

void
import_graphviz (struct shell *shell, Agraph_t *GVG, struct graph *G)
{
  Agnode_t *n;
  Agedge_t *e;
  struct node *node;
  struct node *head, *tail;
  struct link *link;

  for (n = agfstnode (GVG); n; n = agnxtnode (GVG, n))
    {
      node = node_get_by_name (n->name, G);
      node_import_position (node, n);

      for (e = agfstedge (GVG, n); e; e = agnxtedge (GVG, e, n))
        {
          head = node_get_by_name (e->head->name, G);
          tail = node_get_by_name (e->tail->name, G);

          link = link_get (tail, head, G);
        }
    }
}

void
import_graphviz_position (struct shell *shell, Agraph_t *GVG, struct graph *G)
{
  Agnode_t *n;
  struct node *node;

  for (n = agfstnode (GVG); n; n = agnxtnode (GVG, n))
    {
      node = node_lookup_by_name (n->name, G);
      if (! node)
        {
          fprintf (shell->terminal, "graphviz node %s cannot found. "
                   "different graph file ?\n", n->name);
          continue;
        }

      node_import_position (node, n);

      fprintf (shell->terminal, "node %s xpos: %f ypos %f\n",
               node->name, node->xpos, node->ypos);
    }
}

DEFINE_COMMAND (import_graphviz_file,
                "import graphviz file <FILENAME>",
                "import from other data\n"
                "import from GraphViz\n"
                "import information from GraphViz file\n"
                "specify GraphViz filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;

  aginit ();
  GVG = read_graphviz_file (shell, argv[3]);
  if (! GVG)
    {
      fprintf (shell->terminal, "cannot read graphviz file %s\n",
               argv[3]);
      return;
    }

  import_graphviz (shell, GVG, G);
  agclose (GVG);
}

DEFINE_COMMAND (import_graphviz_position,
                "import graphviz position <FILENAME>",
                "import from other data\n"
                "import from GraphViz\n"
                "import position information from GraphViz\n"
                "specify GraphViz filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;

  aginit ();
  GVG = read_graphviz_file (shell, argv[3]);
  if (! GVG)
    {
      fprintf (shell->terminal, "cannot read graphviz file %s\n",
               argv[3]);
      return;
    }

  import_graphviz_position (shell, GVG, G);

  agclose (GVG);
}

DEFINE_COMMAND (import_graphviz_layout,
                "import graphviz layout fdp",
                "import from other data\n"
                "import from GraphViz\n"
                "import from GraphViz layouting\n"
                "import from GraphViz fdp layouting\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;

  graphviz_init (shell, G, &GVG, &agnodes, &agedges);
  graphviz_export (shell, G, GVG, agnodes, agedges, NULL);
  graphviz_layout (GVG, argv[3]);
  import_graphviz_position (shell, GVG, G);
  graphviz_finish (&GVG, &agnodes, &agedges);
}

#endif /*HAVE_GRAPHVIZ*/


