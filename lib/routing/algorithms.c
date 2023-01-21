
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
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/dijkstra.h"
#include "routing/lfi.h"
#include "routing/mara-mc-mmmf.h"
//#include "routing/reverse-dijkstra.h"
#include "routing/mara-spe.h"

void
routing_algorithms_commands (struct command_set *cmdset_routing)
{
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_dijkstra);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_dijkstra_node);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_dijkstra_node_all);

  INSTALL_COMMAND (cmdset_routing, routing_algorithm_lfi);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_lfi_node);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_lfi_node_all);

  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mc);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mc_node);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mc_node_all);

  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mmmf);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mmmf_node);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_mmmf_node_all);

  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_spe);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_spe_node);
  INSTALL_COMMAND (cmdset_routing, routing_algorithm_mara_spe_node_all);
}


