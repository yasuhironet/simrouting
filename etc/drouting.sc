
graph 100
  import brite etc/topology/RTBarabasi20CONS.brite
  show graph structure
exit

weight 100
 weight-graph 100
 weight-setting inverse-capacity
 show weight
exit

routing 100
 routing-graph 100
 routing-weight 100
 routing-algorithm deflection
exit

routing 200
  routing-graph 100
  routing-algorithm ma-ordering
exit


network 100
 simulation deflection 100 drouting 200 ntrials 1000 failure node 1
exit


