#!/bin/bash

WAFDIR=../
WAF=${WAFDIR}/waf
CONTSIZE=./content-size-generator

CFLAG=1
PFLAG=1
NFLAG=1
SFLAG=10

function usage() {
echo "Tiny script to automize running of a ns3 scenario"
echo "Options:"
echo "    -c NUM    Number of clients (consumers) for the scenario. Default [$CFLAG]"
echo "    -p NUM    Number of servers (producers) for the scenario. Default [$PFLAG]"
echo "    -n NUM    Number of networks the scenario will have. Default [$NFLAG]"
echo "    -s NUM    Size, in MB of the content to be distributed. Default [$SFLAG]"
echo ""
}

while getopts "c:s:n:p:h" OPT
do
    case $OPT in
    c)
        CFLAG=$OPTARG  
        ;;
    p)
        PFLAG=$OPTARG
        ;;
    n)
        NFLAG=$OPTARG
        ;;
    s)
        SFLAG=$OPTARG
        ;;
    \?)
        echo "Invalid option: -$OPTARG" >&2
        exit 1
        ;;
    :)
        echo "Option -$OPTARG requires an argument." >&2
        exit 1
        ;;
    h)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
    esac
done

if [ ! -x $CONTSIZE ]; then
    echo "No $CONTSIZE! Compiling..."
    make
fi

BYTES=$($CONTSIZE --avg $SFLAG)

$WAF --run "disaster-tcp --clients=$CFLAG --contentsize=$BYTES --networks=$NFLAG --producers=$PFLAG"
$WAF --run "nms-disaster-ccn --clients=$CFLAG --contentsize=$BYTES --networks=$NFLAG --producers=$PFLAG"
