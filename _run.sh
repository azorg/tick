#! /bin/sh

./_make.sh

echo ">>> run ./tick"
sudo nice --19 ./tick -r -d 1 > data.txt 
