#!/bin/bash

ARRAY=( 50000 100000 150000 200000 250000 300000 350000 400000 )
for i in "${ARRAY[@]}"
do
    selectivity=`echo "${i}/2000000*100"| bc -l`

    Pattern=`grep "tag${i}:" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}"|grep -v ":${i}" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}:" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}"|grep -v ":${i}" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}:" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}"|grep -v ":${i}" | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagName_Equals" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "tag${i}\*:" $1  |grep -v "*tag${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}\*:" $1  |grep -v "*tag${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}\*:" $1  |grep -v "*tag${i}" | grep -v ":${i}" | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagName_Prefix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "\*tag${i}:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "\*tag${i}:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "\*tag${i}:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|head -1| awk '{print $9/1000}'`


    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagName_Suffix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "\*tag${i}\*:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "\*tag${i}\*:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "\*tag${i}\*:" $1  |grep -v ":\*${i}" | grep -v ":${i}" | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagName_Infix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

#==========

    Pattern=`grep "tag${i}:${i}\*" $1  |grep -v "\*tag:${i}" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}:${i}\*" $1  |grep -v "\*tag:${i}" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}:${i}\*" $1  |grep -v "\*tag:${i}" | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagValue_Prefix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "tag${i}:\*${i}" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}\*" | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}:\*${i}" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}\*" | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}:\*${i}" $1  |grep -v "\*tag${i}:" | grep -v ":\*${i}\*" | sort -k9 -nr|head -1| awk '{print $9/1000}'`


    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagValue_Suffix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "tag${i}:\*${i}\*" $1   | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}:\*${i}\*" $1   | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}:\*${i}\*" $1   | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "TagValue_Infix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`
#==========

    Pattern=`grep "tag${i}:${i}" $1 |grep -v "\*tag${i}" |grep -v ":${i}\*"| sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`
    Total=`grep "tag${i}:${i}" $1 |grep -v "\*tag${i}" |grep -v ":${i}\*"| sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}:${i}" $1 |grep -v "\*tag${i}" |grep -v ":${i}\*"| sort -k9 -nr | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "Both_Equals" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`


    Pattern=`grep "\*tag${i}:\*${i}" $1 | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`

    Total=`grep "\*tag${i}:\*${i}" $1 | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "\*tag${i}:\*${i}" $1 | sort -k9 -nr|head -1| awk '{print $9/1000}'`



    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "Both_Prefix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

    Pattern=`grep "tag${i}\*:${i}\*" $1 | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`

    Total=`grep "tag${i}\*:${i}\*" $1 | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "tag${i}\*:${i}\*" $1 | sort -k9 -nr|head -1| awk '{print $9/1000}'`


    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "Both_Suffix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`


    Pattern=`grep "\*tag${i}\*:\*${i}\*" $1 | sort -k9 -nr|tail -1|awk '{print $(NF-1)}'|tr -d '['|tr -d ']'`

    Total=`grep "\*tag${i}\*:\*${i}\*" $1 | sort -k9 -nr|tail -1| awk '{print $NF*1000}'`
    Server=`grep "\*tag${i}\*:\*${i}\*" $1 | sort -k9 -nr|head -1| awk '{print $9/1000}'`

    printf "%-15s, %-22s, %6.1f, Total, %6.2f, Server, %6.2f, Network, %6.2f\n" "Both_Infix" "${Pattern}" "${selectivity}" "${Total}" "${Server}" `echo "$Total-$Server"|bc -l`

done
