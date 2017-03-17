#! /bin/sh

./_make.sh

echo ">>> run ./tick"
sudo nice --19 ./tick -d 10 > data.txt 
#sudo ./tick -d 10 > data.txt 
