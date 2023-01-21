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

#include "log.h"

#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#define INFINITY_BANDWIDTH DBL_MAX

int
node_cmp (const void *va, const void *vb)
{
  struct node *a = *(struct node **)va;
  struct node *b = *(struct node **)vb;
  return (a->id - b->id);
}

double
CalcBW (struct node *ni, struct node *nj, struct vector *Sij,
        double **b, struct vector ***P, struct routing *R)
{
  struct vector_node *vni, *vnj;
  int i, j, x;
  struct vector *CAP;
  int loop = 0;
  double bw = 0.0;
  struct vector *Pij;
#if 0
  char path[128];
#endif

  //fprintf (stderr, "  calculate bw for node %d (dst: %d)\n", ni->id, nj->id);

  i = ni->id;
  j = nj->id;

  if (i == j)
    {
      bw = INFINITY_BANDWIDTH;
      return bw;
    }

  for (vni = vector_head (Sij); vni; vni = vector_next (vni))
    {
      struct node *nx = (struct node *) vni->data;
      struct link *ix;
      x = nx->id;
      ix = link_get_by_node_id (i, x, R->G);
      assert (ix);

      if (vector_lookup (ni, P[x][j]))
        {
          loop ++;
        }
    }

  if (loop)
    {
      fprintf (stderr, "  loop detected, bw = 0\n");
      return (double) 0;
    }

  Pij = vector_create ();

  for (vni = vector_head (Sij); vni; vni = vector_next (vni))
    {
      struct node *nx = (struct node *) vni->data;
      struct link *ix;
      x = nx->id;
      ix = link_get_by_node_id (i, x, R->G);
      assert (ix);

#if 0
      sprint_nodelist (path, sizeof (path), P[x][j]);
      if (b[x][j] == INFINITY_BANDWIDTH)
        fprintf (stderr, "    bw += %3.2f (linkbw %3.2f, node %d's bw: inf path: %s)\n",
               MIN (ix->bandwidth, b[x][j]), ix->bandwidth, x, path);
      else
        fprintf (stderr, "    bw += %3.2f (linkbw %3.2f, node %d's bw: %3.2f path: %s)\n",
               MIN (ix->bandwidth, b[x][j]), ix->bandwidth, x, b[x][j], path);
#endif
      bw += MIN (ix->bandwidth, b[x][j]);

      CAP = vector_cap (Pij, P[x][j]);
      for (vnj = vector_head (CAP); vnj; vnj = vector_next (vnj))
        {
          struct node *ny = (struct node *) vnj->data;
          int y = ny->id;
          if (bw > b[y][j])
            {
              bw = MIN(bw, b[y][j]);
#if 0
              fprintf (stderr, "    bw: least intermediate: node[%d], %f\n", y, bw);
#endif
            }
        }
      vector_delete (CAP);

      for (vnj = vector_head (P[x][j]); vnj; vnj = vector_next (vnj))
        {
          struct node *ny = (struct node *) vnj->data;
          if (! vector_lookup (ny, Pij))
            vector_add_sort (ny, node_cmp, Pij);
        }
    }

  vector_delete (Pij);

  for (vni = vector_head (Sij); vni; vni = vector_next (vni))
    {
      struct node *nx = (struct node *) vni->data;
      struct link *ix;
      x = nx->id;
      ix = link_get_by_node_id (i, x, R->G);
      assert (ix);

      if (bw < MIN (ix->bandwidth, b[x][j]))
        {
          bw = MAX (bw, MIN (ix->bandwidth, b[x][j]));
#if 0
          fprintf (stderr, "    bw: at least: neighbor %d: %f\n", x, bw);
#endif
        }
    }

  return bw;
}

struct vector *
CalcNodeList (struct node *ni, struct node *nj, struct vector *Sij,
              double **b, struct vector ***P, struct routing *R)
{
  struct vector_node *vni, *vnj;
  int i, j, x;
  int loop = 0;
  struct vector *Pij;

  i = ni->id;
  j = nj->id;

  Pij = vector_create ();

  if (i == j)
    {
      vector_add_sort (ni, node_cmp, Pij);
      return Pij;
    }

  for (vni = vector_head (Sij); vni; vni = vector_next (vni))
    {
      struct node *nx = (struct node *) vni->data;
      struct link *ix;
      x = nx->id;
      ix = link_get_by_node_id (i, x, R->G);
      assert (ix);

      if (vector_lookup (ni, P[x][j]))
        {
          loop ++;
          fprintf (stderr, "calculating node-list for node %d to dest %d\n", ni->id, nj->id);
          fprintf (stderr, "node %d's neighbor %d have path-list including %d\n",
                   i, x, i);
          assert(0);
        }
    }

  if (loop)
    {
      assert(0);
    }

  for (vni = vector_head (Sij); vni; vni = vector_next (vni))
    {
      struct node *nx = (struct node *) vni->data;
      struct link *ix;
      x = nx->id;
      ix = link_get_by_node_id (i, x, R->G);
      assert (ix);

      for (vnj = vector_head (P[x][j]); vnj; vnj = vector_next (vnj))
        {
          struct node *ny = (struct node *) vnj->data;
          if (! vector_lookup (ny, Pij))
            vector_add_sort (ny, node_cmp, Pij);
        }
    }

  /* add itself to the nodelist */
  vector_add_sort (ni, node_cmp, Pij);

  return Pij;
}

void
NodeStatus (char *prefix, struct node *node, struct node *dst, struct vector *nexthops,
            double **b, struct vector ***P, struct routing *R)
{
  char path[128];
  char nexthop_str[128];
  char bandwidth[64];
  if (b[node->id][dst->id] == INFINITY_BANDWIDTH)
    snprintf (bandwidth, sizeof (bandwidth), "inf");
  else
    snprintf (bandwidth, sizeof (bandwidth), "%f", b[node->id][dst->id]);
  sprint_nodelist (nexthop_str, sizeof (nexthop_str), nexthops);
  sprint_nodelist (path, sizeof (path), P[node->id][dst->id]);
  fprintf (stderr, "%s node %d: nexthops: %s bw: %s path: %s\n",
           prefix, node->id, nexthop_str, bandwidth, path);
}

void
LogNodeStatus (FILE *fp, char *prefix, struct node *node, struct node *dst,
            struct vector *nexthops,
            double **b, struct vector ***P, struct routing *R)
{
  char path[128];
  char nexthop_str[128];
  char bandwidth[64];
  if (b[node->id][dst->id] == INFINITY_BANDWIDTH)
    snprintf (bandwidth, sizeof (bandwidth), "inf");
  else
    snprintf (bandwidth, sizeof (bandwidth), "%f", b[node->id][dst->id]);
  sprint_nodelist (nexthop_str, sizeof (nexthop_str), nexthops);
  sprint_nodelist (path, sizeof (path), P[node->id][dst->id]);
  fprintf (fp, "%s node %d: nexthops: %s bw: %s path: %s\n",
           (prefix ? prefix : ""), node->id, nexthop_str, bandwidth, path);
}

void
ChangeLog (char *prefix, struct node *ni, struct node *nj, struct vector *Sij,
           double **b, struct vector ***P, struct routing *R,
           double oldb, struct vector *oldP)
{
  char opath[128], npath[128];
  char newprefix[64];
  sprint_nodelist (opath, sizeof (opath), oldP);
  sprint_nodelist (npath, sizeof (npath), P[ni->id][nj->id]);
  fprintf (stderr, "%s update node %d:", prefix, ni->id);
  if (b[ni->id][nj->id] != oldb)
    fprintf (stderr, " bw: %f->%f", oldb, b[ni->id][nj->id]);
  if (! vector_is_same (P[ni->id][nj->id], oldP))
    fprintf (stderr, " path: [%s] -> [%s]", opath, npath);
  fprintf (stderr, "\n");
  snprintf (newprefix, sizeof (newprefix), "%s   ", prefix);
  NodeStatus (newprefix, ni, nj, Sij, b, P, R);
}


int
Update (struct node *ni, struct node *nj, struct vector *Sij,
        double **b, struct vector ***P, struct routing *R)
{
  int i = ni->id;
  int j = nj->id;
  struct vector_node *vn;
  struct node *nk;
  int k;
  double total;
  struct nexthop *nexthop;
  struct link *ik;
  double bw;
  struct vector *Pij;

  if (ni == nj)
    return 0;

  bw = b[i][j];
  b[i][j] = CalcBW (ni, nj, Sij, b, P, R);
  Pij = P[i][j];
  P[i][j] = CalcNodeList (ni, nj, Sij, b, P, R);

  if (b[i][j] == bw && vector_is_same (P[i][j], Pij))
    {
      vector_delete (Pij);
      return 0;
    }

#if 0
  /* logging */
  {
    char opath[128], npath[128];
    sprint_nodelist (opath, sizeof (opath), Pij);
    sprint_nodelist (npath, sizeof (npath), P[i][j]);
    fprintf (stderr, "UPDATE NODE %d:", i);
    if (b[i][j] != bw)
      fprintf (stderr, " bw: %f->%f", bw, b[i][j]);
    if (! vector_is_same (P[i][j], Pij))
      fprintf (stderr, " path: [%s] -> [%s]", opath, npath);
    fprintf (stderr, "\n");
  }
#endif

  /* Update route ratio */
  total = 0.0;
  for (vn = vector_head (Sij); vn; vn = vector_next (vn))
    {
      nk = (struct node *) vn->data;
      k = nk->id;
      ik = link_get_by_node_id (i, k, R->G);
      assert (ik);
      total += MIN(ik->bandwidth, b[k][j]);
    }

  nexthop_delete_all (R->route[i][j].nexthops);
  for (vn = vector_head (Sij); vn; vn = vector_next (vn))
    {
      nk = (struct node *) vn->data;
      k = nk->id;
      ik = link_get_by_node_id (i, k, R->G);
      assert (ik);
      nexthop = nexthop_create ();
      nexthop->node = nk;
      nexthop->ratio = MIN(ik->bandwidth, b[k][j]) / total;
      vector_add (nexthop, R->route[i][j].nexthops);
    }

  vector_delete (Pij);
  return 1;
}

void
debug_func ()
{
}

void
routing_lbra (struct routing *R)
{
  int i, j;
  struct vector ***S;
  double **b;
  struct vector ***P;
  int ichange, kchange;
  struct node *ni, *nj;
  struct timeval start, end;
  int propagate;
  int tiebreak;

  struct vector_node *vni, *vnj;
  struct vector *changed_node = NULL;

  char *dir;
  char previbw[16], ibw[16], prevkbw[16], kbw[16];
  char sbi[16], sdi[16], sbk[16], sdk[16];

  S = (struct vector ***) malloc (sizeof (struct vector **) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    S[i] = (struct vector **) malloc (sizeof (struct vector *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      S[i][j] = vector_create ();

  b = (double **) malloc (sizeof (double *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    b[i] = (double *) malloc (sizeof (double) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    memset (b[i], 0,  sizeof (double) * R->nnodes);

  P = (struct vector ***) malloc (sizeof (struct vector **) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    P[i] = (struct vector **) malloc (sizeof (struct vector *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      P[i][j] = vector_create ();

  /* INITIALIZE */
  for (i = 0; i < R->nnodes; i++)
    {
      ni = node_get (i, R->G);
      for (j = 0; j < R->nnodes; j++)
        {
          nj = node_get (j, R->G);
          if (i == j)
            {
              vector_add_sort (ni, node_cmp, S[i][j]);
              b[i][j] = INFINITY_BANDWIDTH;
              vector_add (ni, P[i][j]);
            }
          Update (ni, nj, S[i][j], b, P, R);
        }
    }

  gettimeofday (&start, NULL);

  for (j = 0; j < R->nnodes; j++)
    {
      nj = node_get (j, R->G);
      j = nj->id;

      fprintf (stderr, "For destination: %d\n", j);

      if (changed_node)
        vector_delete (changed_node);
      changed_node = vector_create ();
      vector_add (nj, changed_node);

      //for (vni = vector_head (changed_node); vni; vni = vector_next (vni))
      while ((vni = vector_head (changed_node)) != NULL)
        {
          ni = (struct node *) vni->data;
          assert (ni);
          i = ni->id;

          fprintf (stderr, "  changed_node: [%d]: ", i);
          print_nodelist (stderr, changed_node);
          fprintf (stderr, "\n");

          vector_remove (ni, changed_node);

          for (vnj = vector_head (ni->olinks); vnj; vnj = vector_next (vnj))
            {
              struct link *olink = (struct link *) vnj->data;
              struct node *nk = olink->to;
              int k = nk->id;
              double bi, bk, nbi, nbk, di, dk;
              struct vector *Sij, *Skj;
              double pbi, pbk;
              struct vector *Pi, *Pk;

#if 1
              fprintf (stderr, "  check link (%d,%d)\n", ni->id, nk->id);
              if (j == 0 && i == 2 && k == 3)
                debug_func ();

              NodeStatus ("  before", ni, nj, S[i][j], b, P, R);
              NodeStatus ("  before", nk, nj, S[k][j], b, P, R);
#endif

              pbi = b[i][j];
              pbk = b[k][j];
              Pi = vector_copy (P[i][j]);
              Pk = vector_copy (P[k][j]);

              if (vector_lookup (nk, S[i][j]))
                vector_remove (nk, S[i][j]);
              if (vector_lookup (ni, S[k][j]))
                vector_remove (ni, S[k][j]);

              Update (ni, nj, S[i][j], b, P, R);
              Update (nk, nj, S[k][j], b, P, R);

              bi = b[i][j];
              bk = b[k][j];

              Sij = vector_copy (S[i][j]);
              vector_add (nk, Sij);
              nbi = CalcBW (ni, nj, Sij, b, P, R);
              vector_delete (Sij);

              Skj = vector_copy (S[k][j]);
              vector_add (ni, Skj);
              nbk = CalcBW (nk, nj, Skj, b, P, R);
              vector_delete (Skj);

              NodeStatus ("  after", ni, nj, S[i][j], b, P, R);
              NodeStatus ("  after", nk, nj, S[k][j], b, P, R);

              di = nbi - bi;
              dk = nbk - bk;

              if (bi == INFINITY_BANDWIDTH)
                snprintf (sbi, sizeof (sbi), "infinity");
              else
                snprintf (sbi, sizeof (sbi), "%.4f", bi);
              snprintf (sdi, sizeof (sdi), "%.4f", di);

              if (bk == INFINITY_BANDWIDTH)
                snprintf (sbk, sizeof (sbk), "infinity");
              else
                snprintf (sbk, sizeof (sbk), "%.4f", bk);
              snprintf (sdk, sizeof (sdk), "%.4f", dk);

              tiebreak = 0;
              dir = "--";
              if (bi < bk)
                {
                  if (di >= 0)
                    {
                      vector_add_sort (nk, node_cmp, S[i][j]);
                      dir = "->";
                    }
                  else if (dk >= 0)
                    {
                      vector_add_sort (ni, node_cmp, S[k][j]);
                      dir = "<-";
                    }
                }
              else if (bi > bk)
                {
                  if (dk >= 0)
                    {
                      vector_add_sort (ni, node_cmp, S[k][j]);
                      dir = "<-";
                    }
                  else if (di >= 0)
                    {
                      vector_add_sort (nk, node_cmp, S[i][j]);
                      dir = "->";
                    }
                }
              else if (bi == bk)
                {
                  if (di > dk && di >= 0)
                    {
                      vector_add_sort (nk, node_cmp, S[i][j]);
                      dir = "->";
                    }
                  else if (dk > di && dk >= 0)
                    {
                      vector_add_sort (ni, node_cmp, S[k][j]);
                      dir = "<-";
                    }
#if 1
                  else if (di == dk && di >= 0)
                    {
                      tiebreak = 1;

#define BALANCE_TIEBREAK
#ifndef BALANCE_TIEBREAK
                      if (i > k)
                        {
                          vector_add_sort (nk, node_cmp, S[i][j]);
                          dir = "->";
                        }
                      else
                        {
                          vector_add_sort (ni, node_cmp, S[k][j]);
                          dir = "<-";
                        }
#else /*BALANCE_TIEBREAK*/
                      if (j % 2)
                        {
                          if (i > k)
                            {
                              vector_add_sort (nk, node_cmp, S[i][j]);
                              dir = "->";
                            }
                          else
                            {
                              vector_add_sort (ni, node_cmp, S[k][j]);
                              dir = "<-";
                            }
                        }
                      else
                        {
                          if (i < k)
                            {
                              vector_add_sort (nk, node_cmp, S[i][j]);
                              dir = "->";
                            }
                          else
                            {
                              vector_add_sort (ni, node_cmp, S[k][j]);
                              dir = "<-";
                            }
                        }
#endif /*BALANCE_TIEBREAK*/

                    }
#endif
                }

              if (pbi == INFINITY_BANDWIDTH)
                snprintf (previbw, sizeof (previbw), "infinity");
              else
                snprintf (previbw, sizeof (previbw), "%.4f", pbi);

              if (pbk == INFINITY_BANDWIDTH)
                snprintf (prevkbw, sizeof (prevkbw), "infinity");
              else
                snprintf (prevkbw, sizeof (prevkbw), "%.4f", pbk);

              Update (ni, nj, S[i][j], b, P, R);
              Update (nk, nj, S[k][j], b, P, R);

              ichange = 0;
              if (b[i][j] != pbi || ! vector_is_same (P[i][j], Pi))
                {
                  ichange = 1;
                  if (! vector_lookup (ni, changed_node))
                    vector_add (ni, changed_node);
                }

              kchange = 0;
              if (b[k][j] != pbk || ! vector_is_same (P[k][j], Pk))
                {
                  kchange = 1;
                  if (! vector_lookup (nk, changed_node))
                    vector_add (nk, changed_node);
                }

              if (b[i][j] == INFINITY_BANDWIDTH)
                snprintf (ibw, sizeof (ibw), "infinity");
              else
                snprintf (ibw, sizeof (ibw), "%.4f", b[i][j]);

              if (b[k][j] == INFINITY_BANDWIDTH)
                snprintf (kbw, sizeof (kbw), "infinity");
              else
                snprintf (kbw, sizeof (kbw), "%.4f", b[k][j]);

#if 1
              /* logging */
              fprintf (stderr, "  direction %d %s %d\n", i, dir, k);

              if (ichange)
                ChangeLog ("    ", ni, nj, S[i][j], b, P, R, pbi, Pi);

              if (kchange)
                ChangeLog ("    ", nk, nj, S[k][j], b, P, R, pbk, Pk);

#else /*1*/
              fprintf (stderr, "    [%d]: %d%s%d(%.4f): i:%d,%s,%s,%s,%s k:%d,%s,%s,%s,%s ",
                       j, i, dir, k, olink->bandwidth,
                       ichange, previbw, sbi, sdi, ibw,
                       kchange, prevkbw, sbk, sdk, kbw);
              fprintf (stderr, "S^%d_%d:[", i, j);
              print_nodelist (stderr, S[i][j]);
              fprintf (stderr, "] P^%d_%d:[", i, j);
              print_nodelist (stderr, P[i][j]);
              fprintf (stderr, "] S^%d_%d:[", k, j);
              print_nodelist (stderr, S[k][j]);
              fprintf (stderr, "] P^%d_%d:[", k, j);
              print_nodelist (stderr, P[k][j]);
              fprintf (stderr, "]%s\n", (tiebreak ? "tiebreak" : ""));
#endif /*0*/

              if (ichange || kchange)
                fprintf (stderr, "      propagate %d-%d change:\n", i, k);

              if (ichange || kchange)
                propagate = 1;
              else
                propagate = 0;
              while (propagate)
                {
                  int updated;
                  struct vector_node *vn;
                  propagate = 0;
                  for (vn = vector_head (R->G->nodes); vn; vn = vector_next (vn))
                    {
                      struct node *node = (struct node *) vn->data;
                      double pb = b[node->id][j];
                      struct vector *pP = vector_copy (P[node->id][j]);
                      updated = Update (node, nj, S[node->id][j], b, P, R);
                      if (updated)
                        ChangeLog ("        ", node, nj, S[node->id][j], b, P, R, pb, pP);
                      propagate += updated;
                    }
                }

            }
        }
    }

  gettimeofday (&end, NULL);
  fprintf (stderr, "Start Time: %lu.%06lu\n", start.tv_sec, start.tv_usec);
  fprintf (stderr, "  End Time: %lu.%06lu\n", end.tv_sec, end.tv_usec);
  fprintf (stderr, "Time taken by LBRA: %lu.%06lu\n",
           end.tv_sec - start.tv_sec - (end.tv_usec < start.tv_usec ? 1 : 0),
           (end.tv_usec < start.tv_usec ? 1000000 : 0) + end.tv_usec - start.tv_usec);

  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      {
        fprintf (stderr, "s: %d t: %d, NodeList: ", i, j);
        print_nodelist (stderr, P[i][j]);
        fprintf (stderr, "\n");
      }

  for (i = 0; i < R->nnodes; i++)
    {
      for (j = 0; j < R->nnodes; j++)
        {
          if (b[i][j] == INFINITY_BANDWIDTH)
            fprintf (stderr, " %4s", "inf");
          else
            fprintf (stderr, " %4.0f", b[i][j]);
        }
      fprintf (stderr, "\n");
    }
}



FILE *logfile = NULL;

void
routing_lbra2 (struct routing *R)
{
  struct vector ***S;  /* NxN matrix of Successor-list */
  double **B;          /* NxN matrix of feasible bandwidth */
  struct vector ***P;  /* NxN matrix of Path-list */
  struct node *node_i, *node_j, *node_k, *node_l;
  struct timeval start, end;
  int i, j, k, l;
  int updated;

  double pBi, pBk, Bi, Bk, nBi, nBk, di, dk;
  struct vector *pSi, *pSk, *pPi, *pPk, *Si, *Sk;

  /* NxN matrix of successor-list */
  S = (struct vector ***) malloc (sizeof (struct vector **) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    S[i] = (struct vector **) malloc (sizeof (struct vector *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      S[i][j] = vector_create ();

  /* NxN matrix of feasible bandwidth */
  B = (double **) malloc (sizeof (double *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    B[i] = (double *) malloc (sizeof (double) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    memset (B[i], 0,  sizeof (double) * R->nnodes);

  /* NxN matrix of path-list */
  P = (struct vector ***) malloc (sizeof (struct vector **) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    P[i] = (struct vector **) malloc (sizeof (struct vector *) * R->nnodes);
  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      P[i][j] = vector_create ();

  /* INITIALIZE */
  for (i = 0; i < R->nnodes; i++)
    {
      node_i = node_get (i, R->G);
      for (j = 0; j < R->nnodes; j++)
        {
          node_j = node_get (j, R->G);
          if (i == j)
            {
              vector_add_sort (node_i, node_cmp, S[i][j]);
              B[i][j] = INFINITY_BANDWIDTH;
              vector_add (node_i, P[i][j]);
            }
          Update (node_i, node_j, S[i][j], B, P, R);
        }
    }

  gettimeofday (&start, NULL);

  for (j = 0; j < R->nnodes; j++)
    {
      node_j = node_get (j, R->G);
      fprintf (stderr, "For destination: %d\n", j);

      for (i = 0; i < R->nnodes; i++)
        for (k = i + 1; k < R->nnodes; k++)
          {
            struct link *link;
            link = link_lookup_by_node_id (i, k, R->G);
            if (link == NULL)
              continue;

            fprintf (stderr, " checking link %d-%d\n", i, k);

            node_i = node_get (i, R->G);
            node_k = node_get (k, R->G);

            /* backup previous state */
            pBi = B[i][j];
            pBk = B[k][j];
            pSi = vector_copy (S[i][j]);
            pSk = vector_copy (S[k][j]);
            pPi = vector_copy (P[i][j]);
            pPk = vector_copy (P[k][j]);

            /* reset link direction */
            if (vector_lookup (node_k, S[i][j]))
              vector_remove (node_k, S[i][j]);
            if (vector_lookup (node_i, S[k][j]))
              vector_remove (node_i, S[k][j]);
            Update (node_i, node_j, S[i][j], B, P, R);
            Update (node_k, node_j, S[k][j], B, P, R);

            /* calculate i -> k (-> ... )-> j */
            Bi = B[i][j];
            Si = vector_copy (S[i][j]);
            vector_add (node_k, Si);
            nBi = CalcBW (node_i, node_j, Si, B, P, R);
            vector_delete (Si);
            di = nBi - Bi;

            /* calculate k -> i (-> ... )-> j */
            Bk = B[k][j];
            Sk = vector_copy (S[k][j]);
            vector_add (node_i, Sk);
            nBk = CalcBW (node_k, node_j, Sk, B, P, R);
            vector_delete (Sk);
            dk = nBk - Bk;

            /* decide whether i->k or k->i */
            if (Bi < Bk)
              {
                if (di > 0)
                  vector_add_sort (node_k, node_cmp, S[i][j]);
                else if (dk > 0)
                  vector_add_sort (node_i, node_cmp, S[k][j]);
              }
            else if (Bi > Bk)
              {
                if (dk > 0)
                  vector_add_sort (node_i, node_cmp, S[k][j]);
                else if (di > 0)
                  vector_add_sort (node_k, node_cmp, S[i][j]);
              }
            else if (Bi == Bk)
              {
                if (di > dk && di > 0)
                  vector_add_sort (node_k, node_cmp, S[i][j]);
                else if (dk > di && dk > 0)
                  vector_add_sort (node_i, node_cmp, S[k][j]);
                else if (di == dk && di > 0)
                  {
                    if (j % 2)
                      {
                        if (i > k)
                          vector_add_sort (node_k, node_cmp, S[i][j]);
                        else
                          vector_add_sort (node_i, node_cmp, S[k][j]);
                      }
                    else
                      {
                        if (i < k)
                          vector_add_sort (node_k, node_cmp, S[i][j]);
                        else
                          vector_add_sort (node_i, node_cmp, S[k][j]);
                      }
                  }
              }

            /* update to the new state (bw, successor-list, etc) */
            Update (node_i, node_j, S[i][j], B, P, R);
            Update (node_k, node_j, S[k][j], B, P, R);

            if (pBi != B[i][j] || ! vector_is_same (pSi, S[i][j]) ||
                ! vector_is_same (pPi, P[i][j]))
              LogNodeStatus (stderr, NULL, node_i, node_j, S[i][j], B, P, R);
            if (pBi != B[i][j] || ! vector_is_same (pSi, S[i][j]) ||
                ! vector_is_same (pPi, P[i][j]))
              LogNodeStatus (stderr, NULL, node_k, node_j, S[k][j], B, P, R);

            /* recalculate all-nodes states (propagate the change) */
            do {
              updated = 0;
              for (l = 0; l < R->nnodes; l++)
                {
                  int ret;
                  node_l = node_get (l, R->G);
                  ret = Update (node_l, node_j, S[l][j], B, P, R);
                  if (ret)
                    LogNodeStatus (stderr, "  propagate", node_l, node_j, S[l][j], B, P, R);
                  updated += ret;
                }
            } while (updated != 0);
          }
    }

  gettimeofday (&end, NULL);
  fprintf (stderr, "Start Time: %lu.%06lu\n", start.tv_sec, start.tv_usec);
  fprintf (stderr, "  End Time: %lu.%06lu\n", end.tv_sec, end.tv_usec);
  fprintf (stderr, "Time taken by LBRA: %lu.%06lu\n",
           end.tv_sec - start.tv_sec - (end.tv_usec < start.tv_usec ? 1 : 0),
           (end.tv_usec < start.tv_usec ? 1000000 : 0) + end.tv_usec - start.tv_usec);

  for (i = 0; i < R->nnodes; i++)
    for (j = 0; j < R->nnodes; j++)
      {
        fprintf (stderr, "s: %d t: %d, NodeList: ", i, j);
        print_nodelist (stderr, P[i][j]);
        fprintf (stderr, "\n");
      }

  for (i = 0; i < R->nnodes; i++)
    {
      for (j = 0; j < R->nnodes; j++)
        {
          if (B[i][j] == INFINITY_BANDWIDTH)
            fprintf (stderr, " %4s", "inf");
          else
            fprintf (stderr, " %4.0f", B[i][j]);
        }
      fprintf (stderr, "\n");
    }
}

DEFINE_COMMAND (routing_algorithm_lbra,
                "routing-algorithm lbra",
                "calculate routing using algorithm\n"
                "Load Balancing Routing Algorithm\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }
  routing_lbra (routing);
  //routing_lbra2 (routing);
}

