#!/bin/bash
BB='../bulletinBoard -T'
RC="../LRC -T -n C1"
LB="../lightBulb -T -n L1"
CF="../configurator \"C1:L1\""

source $(dirname $0)/run_test.sh