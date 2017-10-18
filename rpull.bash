#! /bin/bash
adir=`basename "$PWD"`
rsync -vra joshua.shepherd@pleiades.tricities.wsu.edu:~/$adir/* .
