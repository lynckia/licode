#!/bin/bash
SCRIPT=`pwd`/$0

FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
NVM_CHECK="$PATHNAME"/checkNvm.sh
ROOT=$PATHNAME/..
NVM_CHECK="$ROOT"/scripts/checkNvm.sh

. $NVM_CHECK

cd $ROOT/spine
node runSpineClients -s ../results/config_${TESTPREFIX}_${TESTID}.json -t $DURATION -i 10 -o $ROOT/results/output_${TESTPREFIX}_${TESTID}.json > /dev/null 2>&1
exit 0
