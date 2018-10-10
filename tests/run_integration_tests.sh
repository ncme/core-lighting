#!/bin/bash
echo "Running integration tests:"
COUNT=0
SUCC=0
for test in integration/test-*.sh
do
 ((COUNT++))
 (exec $test) >> /dev/null
 if [ $? == 0 ]; then
    ((SUCC++))
	echo -e "\e[92m * $(basename $test) passed.\e[0m"
 else
    echo -e "\e[91m * $(basename $test) failed.\e[0m"
 fi
done
echo "$SUCC/$COUNT tests passed."
