deriver-archive
===============

<a href="http://archive.org">Internet Archive's</a> setup for creating h.264, creating ogg theora, and creating thumbnails

This is intended to allow an external user to derive video files in the same way
(and using most of the same code) that Internet Archive does.


The least amount of work to get this working is to be on linux ubuntu
or macOSX.

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

./derive [source video filename]



NOTE: if you are interested in extracting recorded TV captions embedded in MPEG-TS files
(eg: "line 21" (AKA "EIA-608")) then I've included "ccextractor-README.txt".


# to setup a readonly version of this on your machine:
git clone git://github.com/traceypooh/deriver-archive.git
