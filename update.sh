#!/bin/bash -ex

cp /petabox/www/common/Arg.inc .
cp /petabox/www/common/Video.inc .

cp /petabox/deriver/derivations.xml  .
cp /petabox/deriver/inc/Thumbnails.php   Thumbnails.inc
cp /petabox/deriver/inc/MPEG4.php  MPEG4.inc

cp /petabox/bin/ffmpeg-README.sh  .

cp /petabox/lib/ffmpeg/ffmpeg-aac.patch .
cp /petabox/lib/ffmpeg/ffmpeg-thumbnails.patch .
