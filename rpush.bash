#! /bin/bash

server=69.166.61.14

adir=`basename "$PWD"`
rsync -vra --delete --exclude=".*" --exclude=".*/" . joshua.shepherd@$server:~/$adir
