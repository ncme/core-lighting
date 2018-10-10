#!/bin/bash
BB="../bulletinBoard"
C1="../LRC -T -P 5688 -n nC1 -p pC1 -l lC1 -R rC1"
C2="../LRC -T -P 5689 -n nC2 -p pC2 -l lC2 -R rC2"
L1="../lightBulb -T -P 5690 -n nL1 -p pL1 -l lL1 -R rL1"
L2="../lightBulb -T -P 5691 -n nL2 -p pL2 -l lL2 -R rL2"
S12="../configurator \"nC1:nL2\""
S21="../configurator \"nC2:nL1\""

eval "$BB >> /dev/null &"
P1=$!
sleep 1
eval "$C1 >> /dev/null &"
P2=$!
sleep 0.5
eval "$C2 >> /dev/null &"
P3=$!
sleep 0.5
eval "$L1 >> /dev/null &"
P4=$!
sleep 0.5
eval "$L2 >> /dev/null &"
P5=$!
sleep 0.5
eval "timeout 5 $S21 >> /dev/null &"
wait $!
eval "timeout 5 $S12 >> /dev/null &"
wait $!
CODE=$?
kill $P1 2> /dev/null
kill $P2 2> /dev/null
kill $P3 2> /dev/null
kill $P4 2> /dev/null
kill $P5 2> /dev/null
exit $CODE