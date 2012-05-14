#!/bin/bash -ex

###############################################################################
# We compile our own ffmpeg because we have config options we want differently
# from the std ubuntu package AND because we have patches.
#
# For example, we want builtin mp3 creation (lame),
# AAC audio encoding and decoding (faac and faad) support,
# and x.264 and ... and a pony 8-) ....
###############################################################################
PET=$HOME/petabox
UBU_NAME=$(lsb_release -cs); # eg: "natty"
DIR=/tmp/f;
PAT=$PET/sw/lib/ffmpeg; # patches
PRESETS=$PET/sw/lib/ffmpeg; # where presets will live


function line(){ perl -e 'print "_"x80; print "\n\n";'; }


if [ "$UBU_NAME" != "natty"  -a  "$UBU_NAME" != "oneiric"  -a  "$UBU_NAME" != "precise" ]; then
    echo "unsupported OS"; exit 1;
fi


mkdir -p $DIR;

sudo apt-get install yasm; # make sure we have an assembler!

# make sure we have basic needed pkgs!
# http://packages.ubuntu.com/search?searchon=contents&arch=any&mode=exactfilename&suite=precise&keywords=libasound.a



sudo apt-get -y install  \
    libschroedinger-dev  libdirac-dev  dirac \
    libx264-dev \
    libfaac-dev \
    libgsm1-dev \
    libmp3lame-dev \
    libspeex-dev \
    libopenjpeg-dev \
    libopencore-amrnb-dev  libopencore-amrwb-dev \
    libvorbis-dev \
    libxvidcore-dev \
    libasound2-dev \
    libavfilter-dev \
    libsdl1.2-dev \
    libtheora-dev \
    libvpx-dev \
    libfreetype6-dev \
    oggz-tools


###############################################################################
# x264 -- make the lib we use to make h.264 video!
#         set it up so that we'll compile it in *statically*
###############################################################################
cd $DIR;
rm -rfv $DIR/usr/;
if [ ! -e x264 ]; then
  git clone git://git.videolan.org/x264.git;
fi
cd x264;
git reset --hard;
git clean -f;
git pull;
git status;
#make clean;





###############################################################################
# ffmpeg -- get source
###############################################################################
function ffmpeg_src()
{
    cd $DIR;

    if [ ! -e ffmpeg ]; then
        git clone git://git.videolan.org/ffmpeg.git;
    fi
    # reverse any mod.s (eg: the patches below!) in case we are rerunning
    cd ffmpeg;
    rm -fv {ffpresets,presets}/*;
    git reset --hard;
    git pull;
    rm -fv 666xx666 $(find . -name '*.orig' -o -name '*.rej');
    line; git status; line;
    # git diff
}



###############################################################################
# ffmpeg -- do 1st pass default config and compile *and* install so
#           we can get modern libavutil, etc. installed in place for x264 build
###############################################################################
ffmpeg_src;
cd $DIR/ffmpeg;
./configure;
make -j4;
env DESTDIR=$DIR  make install;



###############################################################################
# BACK to x264 -- now we can compile it and use most recent libavutil, etc.
#                 (installed above with first ffmpeg config+compile base pass)
###############################################################################
cd $DIR/x264;
./configure --enable-static --enable-pic \
--extra-cflags=-I${DIR?}/usr/local/include \
--extra-ldflags=-L${DIR?}/usr/local/lib

make -j4;
env DESTDIR=$DIR  make install;




###############################################################################
# ffmpeg -- patch
###############################################################################
ffmpeg_src;

# tracey patches:
#    -AAC audio (nondecreasing timestamps is fine; monotonic is too restrictive!)
#    -Better saved thumbnail names
#    -Better quality theora (like "ffmpeg2theora" tool; use both bitrate *and* qscale)
#    -Better copy for MPEG-TS streams (eg: linux recording)
#       NOTE: disabled for now since incompatible w/ mar2012 ffmpeg and 
#             not using MPEG-TS copy at the moment... (and certainly basic MPEG-TS
#             stream copying is working for video w/ detected width/height now...)
cd $DIR/ffmpeg;
for p in $PAT/ffmpeg-aac.patch $PAT/ffmpeg-thumbnails.patch $PAT/ffmpeg-theora.patch; do # $PAT/ffmpeg-copy.patch
  patch -p1 < $p;
done






###############################################################################
# ffmpeg -- compile
###############################################################################
cd $DIR;
cd ffmpeg;
# NOTE: options are alphabetized, thankyouverymuch (makes comparison easier)
# NOTE: --enable-version3  is for prores decoding

./configure $(echo "
--disable-shared
--disable-ffserver
--disable-vdpau

--enable-avfilter
--enable-gpl
--enable-libfaac
--enable-libfreetype
--enable-libgsm
--enable-libmp3lame
--enable-libopencore-amrnb
--enable-libopencore-amrwb
--enable-libopenjpeg
--enable-libspeex 
--enable-libtheora
--enable-libvorbis
--enable-libxvid
--enable-nonfree
--enable-postproc
--enable-pthreads
--enable-static

--prefix=/usr         
--enable-version3 
--enable-libvpx 
--enable-libx264

--extra-cflags=-I${DIR?}/usr/local/include
--extra-ldflags=-L${DIR?}/usr/local/lib
--extra-cflags=-static
--extra-ldflags=-static
");

#xxx --enable-libschroedinger # hmm stopped working in natty/oneiric ~oct2011...


make -j4 V=1;
make alltools;


# dont do this because will overstomp /usr/lib/libavcodec* etc
# and we are making a **static** ffmpeg that will not use those shared libs
# anyway (it can make other tools muckup if those libs rev/change out from them)
#
# make install;  



cp ffmpeg               $PET/sw/bin/ffmpeg.$UBU_NAME;
cp ffprobe              $PET/sw/bin/ffprobe.$UBU_NAME;
cp presets/*.ffpreset   $PRESETS/;




set -x;
echo;echo;echo "NOTE: any changes to $PET/sw/{bin,lib} need to be committed..."
