
graph 100
  import brite etc/topology/RTBarabasi20CONS.brite
  show graph structure
exit

routing 100
  routing-graph 100
  routing-algorithm ma-ordering
  show packet forward
exit


