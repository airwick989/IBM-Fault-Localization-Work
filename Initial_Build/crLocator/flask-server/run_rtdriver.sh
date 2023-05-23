#!/bin/bash

cd ./PI-IBM/build/Dpiperf/bin &&
./rtdriver -a 127.0.0.1 -c start $1 -c stop $2