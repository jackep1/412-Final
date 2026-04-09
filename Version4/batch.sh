#!/bin/bash

# Remove any leftover robotSimulOut files
rm robotSimulOut*.txt

numRows=$1
numCols=$2
numRobots=$3
numDoors=$4
numJobs=$5
timeLimit=60  # Default time limit is 60 seconds

# If time limit is specified, set it
if [ "$#" -eq 6 ]; then
    timeLimit=$6
fi

# Compile the code
./build.sh

# Run the code once for each job requested
for ((i=0; i<numJobs; i++))
do
    outputFile="robotSimulOut $((i+1)).txt"
    ./robotsV4 "$numRows" "$numCols" "$numRobots" "$numDoors" "$outputFile" "$timeLimit"
done