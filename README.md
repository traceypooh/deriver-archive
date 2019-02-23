# <a href="https://archive.org">Internet Archive's</a> setup for creating h.264 mp4 and thumbnails.

This is intended to allow an external user to derive video files in the same way
(and using most of the same code) that Internet Archive does.


The least amount of work to get this working is to be on linux ubuntu, or docker.


## Requirements

You'll need certain binary programs and packages like:
* "php"  (command line variant, likely including some extra modules)
    * (you should be able to type "php -i" on a bash/shell when installed)
* "ffmpeg"
    * (see "ffmpeg-README.txt" for how we configure and build our ffmpeg on linux/macosx)
    * (for some videos, may need "ffprobe" from ffmpeg package as well)
* "exiftool" (for video rotation detection.  can be skipped)
* "mplayer" (if attempting to derive from ISO/CDR/IMG type disc images)


You can then make a directory with a video file in it
and then run (on bash/shell):

`./derive [source video filename]`


## Example using docker

### `docker run --rm -it ubuntu:bionic`
`apt-get update`
`apt-get install -y  ffmpeg  exiftool  wget  php  php-mbstring  php-xml  git`
`perl -i -pe 's/short_open_tag =.*/short_open_tag = on/' /etc/php*/*/*/*.ini`
`git clone git://github.com/traceypooh/deriver-archive`
`cd deriver-archive`
`wget archive.org/download/stairs/stairs.avi`
`./derive stairs.avi`
