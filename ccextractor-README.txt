#!/bin/bash -ex

####################################################################
# setup by tracey april 2012
# updated version feb 2015
# see: http://www.ccextractor.org/download-ccextractor.html
####################################################################


wget -O - 'http://sourceforge.net/projects/ccextractor/files/ccextractor/0.79/ccextractor.src.0.79.zip/download' >| cc.zip;

unzip cc.zip;

# quick patch:
D=ccextractor.*/src/gpacmp4;
perl -i -pe 's/(strchr|strstr)\(/$1((char *)/' $D/{url.c,error.c};

cd ccextractor.*/linux/;
./build;

cp ccextractor ../../
cd ../..;


rm -rfv ccextractor.* cc.zip;
exit 0;
