
graph 100
 import brite etc/topology/RTWaxman20UNI.brite
 export graphviz tmp/topology.dot
 show path source 0 destination 4
exit

routing 150
 routing-graph 100
 routing-algorithm lbra
exit

graph 200
 import routing 150 sink-tree destination 4
 export graphviz tmp/routing.dot
 show path source 0 destination 4
 show route-probabilities source 0 link-failure 5 6
exit


