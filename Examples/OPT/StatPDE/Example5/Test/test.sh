#!/bin/bash
if [ $# -ne 1 ]
    then
    echo "Usage: "$0" [Test|Store]"
    exit 1
fi

PROGRAM=../DOpE-OPT-StatPDE-Example5

bash ../../../../test-single.sh $1 $PROGRAM
