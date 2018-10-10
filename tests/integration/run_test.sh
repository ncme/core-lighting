#!/bin/bash
eval "$BB  &"
P1=$!
sleep 0.5
eval "$RC &"
P2=$!
sleep 0.5
eval "$LB >> /dev/null &"
P3=$!
sleep 0.5
eval "timeout 10 $CF &"
wait $!
CODE=$?
kill $P1 2> /dev/null
kill $P2 2> /dev/null
kill $P3 2> /dev/null
exit $CODE
