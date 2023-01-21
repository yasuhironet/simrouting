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

#include <string.h>

#ifdef HAVE_NETSNMP

#include "snmp.h"

struct snmp_session *
snmp_start (int version, char *host, char *community)
{
  struct snmp_session hint, *sess;

  init_snmp ("ospf-snmp");

  snmp_sess_init (&hint);
  hint.version = (version ? version : SNMP_VERSION_2c);
  hint.peername = host;
  hint.community = (u_char *) community;
  hint.community_len = strlen (community);

  sess = snmp_open (&hint);
  if (sess == NULL)
    {
      snmp_perror ("snmp_open ()");
      snmp_log (LOG_ERR, "cannot open session to %s\n", host);
      return NULL;
    }

  return sess;
}

/* Request SNMP MIB "name" with len "restrict_len"
   to the snmp target. The result is stored in "buf". */
void
snmp_get_next (struct snmp_session *sess,
               char *name, int restrict_len,
               char *buf, int *buflen)
{
  struct snmp_pdu *req = NULL;
  struct snmp_pdu *res = NULL;
  oid object_id[MAX_OID_LEN];
  int object_id_len = MAX_OID_LEN;
  struct variable_list *var;
  int i, status;

  memset (object_id, 0, sizeof (object_id));
  printf ("snmp get next: %s\n", name);

  if (snmp_parse_oid (name, object_id, (size_t *)&object_id_len) == NULL)
    {
      snmp_perror (name);
      snmp_log (LOG_ERR, "cannot parse oid %s\n", name);
      *buflen = 0;
      return;
    }

  printf ("oid(len: %d): ", object_id_len);
  for (i = 0; i < object_id_len; i++)
    printf (".%lu", object_id[i]);
  printf ("\n");

  req = snmp_pdu_create (SNMP_MSG_GETNEXT);
  snmp_add_null_var (req, object_id, object_id_len);

  status = snmp_synch_response (sess, req, &res);
  if (status != STAT_SUCCESS)
    {
      snmp_sess_perror ("snmp_synch_response ()", sess);
      *buflen = 0;
      if (res)
        snmp_free_pdu (res);
      snmp_close (sess);
      return;
    }

  if (res->errstat != SNMP_ERR_NOERROR)
    {
      *buflen = 0;
      if (res)
        snmp_free_pdu (res);
      snmp_close (sess);
      return;
    }

  /* does not support bulk transfer yet, though */
  for (var = res->variables; var; var = var->next_variable)
    {
      /* check if var->name is in the (specified) sub-tree */
      if (memcmp (object_id, var->name, restrict_len * sizeof (oid)))
        {
          *buflen = 0;
          break;
        }

      memcpy (buf, var->val.string, MIN (var->val_len, *buflen - 1));
      buf[MIN (var->val_len, *buflen - 1)] = '\0';
      *buflen = MIN (var->val_len, *buflen - 1);
      break;
    }

  snmp_free_pdu (res);
  return;
}

void
snmp_end (struct snmp_session *sess)
{
  snmp_close (sess);
}

#endif /*HAVE_NETSNMP*/

