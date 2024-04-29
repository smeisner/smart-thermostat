#!/bin/bash

# While the thermostat firmware is running, it periodically outputs the
# amount of heap free space. It does this every minute. The script below
# will pares the output log file and generate a CSV that includes
# <time>,<heap size>
# This can then be imported into a spreadsheet and graphed to see trends
# (like identifying memory leaks)

if [ -z "$1" ]
  then
    echo "Specify the name of the log file to analyze. Output will be in <name>.csv"
    exit
fi

filename="${1%.*}"

#egrep '>>>' $1.log | cut -d' ' -f 8 | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" > $1.csv
egrep -a '>>>' $filename.log | cut -d' ' -f 1,8 | cut -d'.' -f 2,3,4,5  | sed 's/h./:/; s/\x6D./:/; s/s././' | sed 's/] /,/' | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" > $filename.csv

echo "Created output file $filename.csv"
