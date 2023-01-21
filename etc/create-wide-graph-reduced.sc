
group fujisawa 10 match domain-suffix .fujisawa.wide.ad.jp
group otemachi 10 match domain-suffix .otemachi.wide.ad.jp
group notemachi 10 match domain-suffix .notemachi.wide.ad.jp
group tokyo 10 match domain-suffix .tokyo.wide.ad.jp
group yagami 10 match domain-suffix .yagami.wide.ad.jp
group osaka 10 match domain-suffix .osaka.wide.ad.jp
group nezu 10 match domain-suffix .nezu.wide.ad.jp
group dojima 10 match domain-suffix .dojima.wide.ad.jp
group sakyo 10 match domain-suffix .sakyo.wide.ad.jp
group kurashiki 10 match domain-suffix .kurashiki.wide.ad.jp
group fukuoka 10 match domain-suffix .fukuoka.wide.ad.jp
group komatsu 10 match domain-suffix .komatsu.wide.ad.jp
group SanFrancisco 10 match domain-suffix .SanFrancisco.wide.ad.jp
group nara 10 match domain-suffix .nara.wide.ad.jp
 
graph 100
 import ospf localhost public 0.0.0.0
 export graphviz tmp/wide-ospf-net-%Y%m%d-%H%M%S-raw.dot
exit

graph 200
 import graph 100
 contract edges both node group fujisawa name fujisawa
 contract edges both node group otemachi name otemachi
 contract edges both node group notemachi name notemachi
 contract edges both node group tokyo name tokyo
 contract edges both node group yagami name yagami
 contract edges both node group osaka name osaka
 contract edges both node group nezu name nezu
 contract edges both node group dojima name dojima
 contract edges both node group sakyo name sakyo
 contract edges both node group kurashiki name kurashiki
 contract edges both node group fukuoka name fukuoka
 contract edges both node group komatsu name komatsu
 contract edges both node group SanFrancisco name SanFrancisco
 contract edges both node group nara name nara
 remove stub-node
 remove stub-node
 realloc identifiers
 save graph config
 export graphviz tmp/wide-ospf-net-%Y%m%d-%H%M%S-noc.dot
exit

write config etc/test.config
exit

