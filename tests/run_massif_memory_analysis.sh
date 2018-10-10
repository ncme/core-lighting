#!/bin/bash
valgrind --tool=massif --stacks=yes --time-unit=ms --detailed-freq=1 ../bulletinBoard -T > /dev/null & 
sleep 0.5
valgrind --tool=massif --stacks=yes --time-unit=ms --detailed-freq=1 ../LRC -T > /dev/null & 
sleep 0.5
valgrind --tool=massif --stacks=yes --time-unit=ms --detailed-freq=1 ../lightBulb -T > /dev/null & 
sleep 0.5
valgrind --tool=massif --stacks=yes --time-unit=ms --detailed-freq=1 ../configurator "LRC 1:Light 1" >/dev/null
wait $!
echo "test complete"
killall valgrind
echo "finished"