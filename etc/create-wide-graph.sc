
graph 100
 import ospf localhost public 0.0.0.0
 export graphviz tmp/wide-ospf-net-%Y%m%d-%H%M%S.dot
 save graph config
exit

weight 100
 weight-graph 100
 import ospf localhost public 0.0.0.0
 export graphviz tmp/wide-ospf-net-%Y%m%d-%H%M%S-weight.dot
 save weight config
exit

write config tmp/wide-ospf-net-%Y%m%d-%H%M%S.config

