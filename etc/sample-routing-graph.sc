
graph 100
  import brite etc/topology/RTBarabasi20CONS.brite
  show graph structure
  export graphviz tmp/sample-barabasi-20.dot
exit

routing 200
  routing-graph 100
  routing-algorithm lbra
  show routing
exit

graph 200
  import routing 200 sink-tree destination 19
  show graph structure
  export graphviz tmp/sample-barabasi-20-lbra-dst19.dot
  show path source 0
  show path source 0 destination 19
  show path source 1 destination 19
  show path source 2 destination 19
  show path source 3 destination 19
  show path source 8 destination 19
  show path source 8 destination 19 probability routing 200
  show path source 8 destination 19 probability routing 200 failure 7 4
  packet forward source 8 destination 19 probability routing 200 failure 7 4 trial 1000
exit

weight 300
 weight-graph 100
 weight-setting minimum-hop
 show weight
exit

routing 300
 routing-graph 100
 routing-weight 300
 routing-algorithm dijkstra
 show routing
exit

graph 300
  import routing 300 sink-tree destination 19
  show graph structure
  export graphviz tmp/sample-barabasi-20-dijkstra-dst19.dot
  show path source 0
  show path source 0 destination 19
  show path source 1 destination 19
  show path source 2 destination 19
  show path source 3 destination 19
  show path source 8 destination 19
exit


