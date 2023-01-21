
graph 100
 import brite ../brite/BRITE/RTBarabasi4.brite
 export graphviz tmp/traffic-engineering.dot
 print ampl data
exit

weight 100
 weight-graph 100
 weight-setting minimum-hop
exit

traffic 100
 traffic-graph 100
 traffic-seed 37
 traffic-model fortz-thorup alpha 300.0
 show traffic
 print ampl data
exit

routing 100
 routing-graph 100
 routing-weight 100
 routing-algorithm dijkstra
 show routing
 print ampl data
exit

network 100
 network-graph 100
 network-routing 100
 network-traffic 100
 network-load traffic-flows
 show network
exit

routing 200
 routing-graph 100
 routing-algorithm ma-ordering
 show routing
 print ampl data
exit

network 200
 network-graph 100
 network-routing 200
 network-traffic 100
 network-load traffic-flows
 show network
exit

graph 110
 import routing 100 sink-tree destination 0
 export graphviz tmp/traffic-engineering-dijkstra.dot
exit

graph 210
 import routing 200 sink-tree destination 0
 export graphviz tmp/traffic-engineering-mara.dot
exit


