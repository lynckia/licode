#!/bin/bash
SCRIPT=`pwd`/$0

FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
NVM_CHECK="$PATHNAME"/checkNvm.sh
ROOT=$PATHNAME/..
NVM_CHECK="$ROOT"/scripts/checkNvm.sh

echo `pwd`
echo $0
echo $NVM_CHECK
echo $PATHNAME
echo $SCRIPT
echo $FILENAME

. $NVM_CHECK

export LD_LIBRARY_PATH=/opt/licode/erizo/build/erizo

cd $ROOT/spine
node runSpineClients -s spineClientsConfig.json -t 10 -i 1 -o $ROOT/results/output.json
