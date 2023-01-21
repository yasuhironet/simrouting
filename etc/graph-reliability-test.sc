
graph 100
  import brite etc/topology/RTBarabasi5.brite
  show graph structure
  link all reliability 0.9
  export graphviz tmp/sample-barabasi-5.dot
  show path source 0
  calculate reliability source 0 destination 4 stat detail
exit

