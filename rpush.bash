#! /bin/bash
adir=`basename "$PWD"`
rsync -vra --delete --exclude=".*" --exclude=".*/" . joshua.shepherd@pleiades.tricities.wsu.edu:~/$adir
