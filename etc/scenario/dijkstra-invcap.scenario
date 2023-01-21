
graph 100
 import brite etc/topology/sample.brite
 export graphviz tmp/brite-dijkstra-invcap.dot
exit

weight 100
 weight-graph 100
 weight-setting inverse-capacity
exit

routing 100
 routing-graph 100
 routing-weight 100
 routing-algorithm dijkstra
exit

traffic 100
 traffic-graph 100
 traffic-seed 30
 traffic-model fortz-thorup alpha 100.0
 show traffic
exit

network 100
 network-graph 100
 network-routing 100
 network-traffic 100
 network-load traffic-flows
 show network
exit


