#!/bin/bash
BB='../bulletinBoard -T -P 16005 -R bananas'
RC="../LRC -T -B coap://127.0.0.1:16005 -P 16006 -R status -n C1"
LB="../lightBulb -T -B coap://127.0.0.1:16005 -P 16007 -R configuration -n L1"
CF="../configurator -B coap://127.0.0.1:16005 \"C1:L1\""

source $(dirname $0)/run_test.sh