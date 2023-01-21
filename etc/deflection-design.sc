
graph 100
  import brite etc/topology/RTBarabasi20UNI.brite
  show graph structure
  export graphviz tmp/sample-barabasi-20.dot
exit

weight 300
 weight-graph 100
 weight-setting inverse-capacity
 show weight
exit

routing 300
 routing-graph 100
 routing-weight 300
 routing-algorithm deflection
 show routing
 show deflection set
 show deflection number
 show deflection deflect-1 path
 show deflection deflect-2 path
 show deflection deflect-3 path
exit


