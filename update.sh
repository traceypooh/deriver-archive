#!/bin/bash -ex

cp /petabox/www/common/Arg.inc .
cp /petabox/www/common/Video.inc .

cp /petabox/deriver/derivations.xml  .
cp /petabox/deriver/inc/Thumbnails.php   Thumbnails.inc
cp /petabox/deriver/inc/MPEG4.php  MPEG4.inc

cp /petabox/bin/ffmpeg-README.txt  .

for i in ffmpeg-aac.patch  ffmpeg-PAT.patch  ffmpeg-thumbnails.patch; do
  rm -f $i
  wget https://archive.org/~tracey/downloads/patches/$i
done