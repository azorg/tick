#! /bin/sh

./_make.sh

echo ">>> run './tick'"
#sudo nice --19 ./tick -r -d 10 > data.txt 
sudo ./tick -r -d 10 > data.txt 
#sudo ./tick -d 10 > data.txt 
