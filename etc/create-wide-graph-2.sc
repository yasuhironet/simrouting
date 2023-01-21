
graph 100
 import ospf localhost public 0.0.0.0
exit

weight 100
 weight-graph 100
 import ospf localhost public 0.0.0.0
 export graphviz tmp/wide-ospf-net-%Y%m%d-%H%M%S-raw.dot domain wide.ad.jp
exit

