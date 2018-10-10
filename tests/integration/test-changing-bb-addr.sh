#!/bin/bash
BB="../bulletinBoard -T -P 16016"
RC="../LRC -T -B coap://127.0.0.1:16016"
LB="../lightBulb -T -B coap://127.0.0.1:16016"
CF="../configurator -B coap://127.0.0.1:16016 \"LRC 1:Light 1\""

source $(dirname $0)/run_test.sh