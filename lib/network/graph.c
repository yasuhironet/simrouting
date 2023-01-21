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
#include "graph.h"

struct node *
node_create (unsigned int id, struct graph *g)
{
  struct node *v;

  v = (struct node *) malloc (sizeof (struct node));
  memset (v, 0, sizeof (struct node));

  v->id = id;
  v->olinks = vector_create ();
  v->ilinks = vector_create ();
  v->g = g;

  return v;
}

void
node_delete (struct node *v)
{
  struct vector_node *vn;

  if (v->name)
    free (v->name);
  if (v->domain_name)
    free (v->domain_name);

  for (vn = vector_head (v->olinks); vn; vn = vector_next (vn))
    {
      struct link *e = (struct link *) vn->data;
      if (e)
        link_delete (e);
    }
  vector_delete (v->olinks);

  for (vn = vector_head (v->ilinks); vn; vn = vector_next (vn))
    {
      struct link *e = (struct link *) vn->data;
      if (e)
        link_delete (e);
    }
  vector_delete (v->ilinks);

  free (v);
}

void
node_add (struct node *v, struct graph *g)
{
  struct node *exist;
  exist = vector_get (g->nodes, v->id);
  if (exist && exist != v)
    node_delete (exist);
  vector_set (g->nodes, v->id, v);
}

void
node_remove (struct node *v, struct graph *g)
{
  assert (v->g == g);
  if (vector_get (g->nodes, v->id) == v)
    vector_set (g->nodes, v->id, NULL);
}

struct node *
node_lookup (unsigned int id, struct graph *g)
{
  return (struct node *) vector_get (g->nodes, id);
}

struct node *
node_lookup_by_name (char *name, struct graph *g)
{
  struct vector_node *vn;
  struct node *node, *match = NULL;
  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vector_data (vn);
      if (! strcmp (name, node->name))
        match = node;
    }
  return match;
}

struct link *
link_create (unsigned int id, struct graph *g)
{
  struct link *e;

  e = (struct link *) malloc (sizeof (struct link));
  memset (e, 0, sizeof (struct link));

  e->g = g;
  e->id = id;

  return e;
}

void
link_delete (struct link *e)
{
  link_remove (e, e->g);

  if (e->from)
    vector_remove (e, e->from->olinks);
  if (e->to)
    vector_remove (e, e->to->ilinks);
  if (e->inverse)
    {
      assert (e->inverse->inverse == e);
      e->inverse->inverse = NULL;
    }

  if (e->name)
    free (e->name);
  free (e);
}

void
link_add (struct link *e, struct graph *g)
{
  struct link *exist;
  exist = vector_get (g->links, e->id);
  if (exist && exist != e)
    link_delete (exist);
  vector_set (g->links, e->id, e);
}

void
link_remove (struct link *e, struct graph *g)
{
  int i;
  assert (e->g == g);
#if 0
  if (vector_get (g->links, e->id) == e)
    vector_set (g->links, e->id, NULL);
#else
  assert (vector_get (g->links, e->id) == e);
  vector_remove (e, g->links);
  for (i = e->id; i < g->links->size; i++)
    {
      struct link *link = vector_get (g->links, i);
      link->id = i;
    }
#endif
}

struct graph *
graph_create ()
{
  struct graph *g;

  g = (struct graph *) malloc (sizeof (struct graph));
  memset (g, 0, sizeof (struct graph));

  g->name = strdup ("simrouting");
  g->nodes = vector_create ();
  g->links = vector_create ();

  g->config = vector_create ();
  return g;
}

void
graph_clear (struct graph *G)
{
  struct vector_node *vn;
  struct link *link;
  struct node *node;

  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (link)
        link_delete (link);
    }

  for (vn = vector_head (G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      if (node)
        node_delete (node);
    }
}

void
graph_delete (struct graph *g)
{
  graph_clear (g);

  vector_delete (g->nodes);
  vector_delete (g->links);

  command_config_clear (g->config);
  vector_delete (g->config);

  free (g->name);
  free (g);
}


struct node *
node_get (unsigned int id, struct graph *g)
{
  struct node *node;

  node = node_lookup (id, g);
  if (node)
    return node;

  node = node_create (id, g);
  node_add (node, g);

  return node;
}

struct node *
node_get_by_name (char *name, struct graph *g)
{
  struct node *node;

  node = node_lookup_by_name (name, g);
  if (node)
    return node;

  node = node_create (vector_empty_index (g->nodes), g);
  node->name = strdup (name);
  node_add (node, g);

  return node;
}

void
node_set (unsigned int id, struct graph *g)
{
  struct node *node;

  node = node_lookup (id, g);
  if (node)
    return;

  node = node_create (id, g);
  node_add (node, g);
}

struct link *
link_lookup_by_id (unsigned int id, struct graph *g)
{
  return (struct link *) vector_get (g->links, id);
}

struct link *
link_lookup (struct node *s, struct node *t, struct graph *g)
{
  struct vector_node *vn;
  struct link *e, *match = NULL;
  for (vn = vector_head (s->olinks); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e && e->to == t)
        match = e;
    }
  return match;
}

struct link *
link_lookup_by_node_id (unsigned int source, unsigned int sink, struct graph *g)
{
  struct link *e;
  struct node *s, *t;
  s = node_get (source, g);
  t = node_get (sink, g);
  e = link_lookup (s, t, g);
  return e;
}

struct link *
link_get_by_id (unsigned int id, struct graph *g)
{
  struct link *e;

  e = link_lookup_by_id (id, g);
  if (e)
    return e;

  e = link_create (id, g);
  link_add (e, g);

  return e;
}

void
link_connect (struct link *e, struct node *s, struct node *t, struct graph *g)
{
  struct link *inverse;

  e->from = s;
  e->to = t;
  vector_add (e, s->olinks);
  vector_add (e, t->ilinks);

  inverse = link_lookup (t, s, g);
  if (inverse && e != inverse)
    {
      e->inverse = inverse;
      inverse->inverse = e;
    }
}

struct link *
link_get (struct node *s, struct node *t, struct graph *g)
{
  struct link *e;

  e = link_lookup (s, t, g);
  if (e)
    return e;

  e = link_get_by_id (vector_empty_index (g->links), g);
  assert (e->from == NULL && e->to == NULL);
  link_connect (e, s, t, g);

  return e;
}

void
link_set (struct node *s, struct node *t, struct graph *g)
{
  struct link *e;

  e = link_lookup (s, t, g);
  if (e)
    return;

  e = link_get_by_id (vector_empty_index (g->links), g);
  assert (e->from == NULL && e->to == NULL);
  link_connect (e, s, t, g);
}

struct link *
link_get_by_node_id (unsigned int source, unsigned int sink, struct graph *g)
{
  struct link *e;
  struct node *s, *t;
  s = node_get (source, g);
  t = node_get (sink, g);
  e = link_get (s, t, g);
  return e;
}

void
link_set_by_node_id (unsigned int source, unsigned int sink, struct graph *g)
{
  struct link *e;
  struct node *s, *t;
  s = node_get (source, g);
  t = node_get (sink, g);
  e = link_get (s, t, g);
}

int
graph_nodes (struct graph *G)
{
  struct vector_node *vn;
  struct node *v;
  int nnodes = 0;
  for (vn = vector_head (G->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (v == NULL)
        continue;
      nnodes++;
    }
  return nnodes;
}

int
graph_edges (struct graph *G)
{
  struct vector_node *vn;
  struct link *e;
  int nedges = 0;
  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e == NULL)
        continue;
      nedges++;
    }
  return nedges;
}

#define STRDUP_REPLACE(dst,src)      \
  if (dst)                           \
    free (dst);                      \
  dst = (src ? strdup (src) : NULL);

struct node *
node_copy_create (struct node *node, struct graph *g)
{
  struct node *copy;

  copy = node_get (node->id, g);
  copy->addr = node->addr;
  copy->plen = node->plen;

  STRDUP_REPLACE (copy->name, node->name);
  STRDUP_REPLACE (copy->domain_name, node->domain_name);
  STRDUP_REPLACE (copy->descr, node->descr);

  copy->xpos = node->xpos;
  copy->ypos = node->ypos;

  copy->type = node->type;

  return copy;
}

struct link *
link_copy_create (struct link *link, struct graph *g)
{
  struct link *copy;
  struct link *inverse;

  copy = link_get_by_node_id (link->from->id, link->to->id, g);

  if (link->inverse)
    {
      inverse = link_lookup_by_node_id (link->to->id, link->from->id, g);
      copy->inverse = inverse;
      inverse->inverse = copy;
    }

  STRDUP_REPLACE (copy->name, link->name);
  STRDUP_REPLACE (copy->descr, link->descr);

  copy->bandwidth = link->bandwidth;
  copy->delay = link->delay;
  copy->length = link->length;

  return copy;
}

struct graph *
graph_copy_create (struct graph *G)
{
  struct graph *copy;
  struct vector_node *vn;
  struct node *node;
  struct link *link;

  copy = graph_create ();

  for (vn = vector_head (G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      if (! node)
        continue;
      node_copy_create (node, copy);
    }

  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (! link)
        continue;
      link_copy_create (link, copy);
    }

  return copy;
}

void
graph_copy (struct graph *dst, struct graph *src)
{
  struct vector_node *vn;
  struct node *node;
  struct link *link;

  graph_clear (dst);

  for (vn = vector_head (src->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      if (! node)
        continue;
      node_copy_create (node, dst);
    }

  for (vn = vector_head (src->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (! link)
        continue;
      link_copy_create (link, dst);
    }
}

void
graph_pack_id (struct graph *G)
{
  struct vector_node *vn;
  struct node *node;
  struct link *link;
  unsigned int new_id;

  for (vn = vector_head (G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      if (! node)
        continue;
      new_id = vector_empty_index (G->nodes);
      if (node->id <= new_id)
        continue;

      vector_set (G->nodes, node->id, NULL);
      node->id = new_id;
      vector_set (G->nodes, node->id, node);
    }

  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (! link)
        continue;
      new_id = vector_empty_index (G->links);
      if (link->id <= new_id)
        continue;

      vector_set (G->links, link->id, NULL);
      link->id = new_id;
      vector_set (G->links, link->id, link);
    }
}

void
graph_print (struct graph *g)
{
  struct node *v;
  struct link *e;
  struct vector_node *vn, *vno;

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (v == NULL)
        continue;
      printf ("node[%d]: ", v->id);
      printf ("neighbor: ");
      for (vno = vector_head (v->olinks); vno; vno = vector_next (vno))
        {
          e = (struct link *) vno->data;
          printf (" %d", e->to->id);
        }
      printf ("\n");
    }

  printf ("Graph Link:\n");
  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e == NULL)
        continue;
      printf ("link[%2d]: %2d-%2d bandwidth: %f\n",
              e->id, e->from->id, e->to->id, e->bandwidth);
    }
}

