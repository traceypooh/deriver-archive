#!/bin/bash -ex

###############################################################################
# author: Tracey Jaquith.  Software license GPL version 2.
# last updated:   $Date: 2012-05-30 23:11:52 +0000 (Wed, 30 May 2012) $
###############################################################################
# We compile our own ffmpeg because we have config options we want differently
# from the std ubuntu package AND because we have patches.
#
# For example, we want builtin mp3 creation (lame),
# AAC audio encoding and decoding (faac and faad) support,
# and h.264/x264 and ... and a pony 8-) ....
###############################################################################
DIR=/tmp/f;
DIRIN=$DIR/usr;
SHORTNAME=mac;

unset LD_LIBRARY_PATH; #avoid any contamination when set!
MYDIR=$(d=`dirname $0`; echo $d | grep '^/' || echo `pwd`/$d);




MYCC="";
MYCC2="";
FFXTRA="";
if [ $(uname -s) == 'Darwin' ]; then
  FFXTRA="
--prefix=/opt/local
--extra-cflags=-I/opt/local/include
--extra-cflags=-I/opt/local/include/SDL

--extra-ldflags=-L/opt/local/lib
";

  DIR=/opt/local/x;
  DIRIN=$DIR/opt;

  if [ "$OSTYPE" != "darwin10.0" ]; then
    MYCC="--cc=clang"; # NOTE: esp. for mac lion!
    MYCC2="--disable-vda"; # NOTE: esp. for mac lion!
  fi
 
  echo "step 1: install  macports -- see http://www.macports.org/install.php"

  # Some generally useful port commands (in "terminal" app):
  #   sudo port -v selfupdate
  #   sudo port upgrade outdated
  #   port installed
  #   port search <portname>
  #   sudo port install <portname>

  sudo port install  git-core lame libtheora libvorbis openjpeg faac bzip2 freetype yasm opencore-amr xvid openjpeg libvpx pkgconfig;
  sudo port install  libsdl  xorg-libXfixes;   # X and SDL stuff for ffplay

else
  FFXTRA="
--prefix=/usr         
--enable-libfreetype
--enable-libgsm
--enable-libspeex
--extra-ldflags=-static
";

  SHORTNAME=$(lsb_release -cs); # eg: "precise"
  if [ "$SHORTNAME" != "natty"  -a  "$SHORTNAME" != "oneiric"  -a  "$SHORTNAME" != "precise"  -a  "$SHORTNAME" != "quantal" ]; then
    echo "unsupported OS"; exit 1;
  fi

  # Make sure we have basic needed pkgs!
  sudo apt-get install yasm; # make sure we have an assembler!

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
    libfreetype6-dev

fi;

function line(){ perl -e 'print "_"x80; print "\n\n";'; }


################################################################################
# ffmpeg, ffprobe, qt-faststart, x264, mplayer
#       compile directly an ffmpeg that can create x264 and WebM
#       and by compiling directly, we can make qt-faststart, too!
################################################################################



sudo mkdir -p $DIR;
sudo chown $USER $DIR;
cd $DIR;





###############################################################################
# x264 -- make the lib we use to make h.264 video!
#         set it up so that we'll compile it in *statically*
###############################################################################
cd $DIR;
rm -rfv $DIRIN;
if [ ! -e x264 ]; then
  git clone git://git.videolan.org/x264.git;
fi
cd x264;
git reset --hard;
git clean -f;
git pull;
git status;



###############################################################################
# ffmpeg -- get source
###############################################################################
function ffmpeg_src()
{
    cd $DIR;

    if [ ! -e ffmpeg ]; then
        git clone git://source.ffmpeg.org/ffmpeg.git;
    fi

    cd ffmpeg;
    git reset --hard;
    git clean -f;
    git pull;
    line; git status; line;
    # git diff
}


###############################################################################
# ffmpeg -- tracey patches
###############################################################################
#    -AAC audio (nondecreasing timestamps is fine; monotonic is too restrictive!)
#    -Better saved thumbnail names
#    -Better quality theora (like "ffmpeg2theora" tool; use both bitrate *and* qscale)
#    -Better copy for MPEG-TS streams (eg: linux recording)
#       NOTE: disabled for now since incompatible w/ mar2012 ffmpeg and 
#             not using MPEG-TS copy at the moment... (and certainly basic MPEG-TS
#             stream copying is working for video w/ detected width/height now...)
function ffmpeg_patch()
{
    cd $DIR/ffmpeg;
    PATDIR=
    for p in ffmpeg-aac.patch  ffmpeg-thumbnails.patch  ffmpeg-theora.patch; do # ffmpeg-copy.patch
        if [ "$PATDIR" == "" ]; then
            # find the patches dir!
            if [ -e "$MYDIR/$p" ]; then
                PATDIR=file://$MYDIR; 
            elif [ -e "$MYDIR/../lib/ffmpeg/$p" ]; then
                PATDIR=file://$MYDIR/../lib/ffmpeg;
            else
                PATDIR=http://archive.org/~tracey/downloads/patches;
            fi
        fi

        curl "$PATDIR/$p" >| ../$p;
        echo APPLYING PATCH $p;
        patch -p1 < ../$p;
    done
}



###############################################################################
# ffmpeg -- do 1st pass default config and compile *and* install so
#           we can get modern libavutil, etc. installed in place for x264 build
###############################################################################
ffmpeg_src;
ffmpeg_patch; # test applying patches *first* in case any need updating
ffmpeg_src;   # revert any of the patches for this first clean/stock build
cd $DIR/ffmpeg;
./configure;
make -j4;
env DESTDIR=$DIR  make install;


###############################################################################
# BACK to x264 -- now we can compile it and use most recent libavutil, etc.
#                 (installed above with first ffmpeg config+compile base pass)
###############################################################################
cd $DIR/x264;
./configure --enable-static --enable-pic --disable-asm \
--extra-cflags=-I${DIRIN?}/local/include \
--extra-ldflags=-L${DIRIN?}/local/lib
# xxxx mac     ./configure --prefix=$DIRIN/local --enable-static --enable-shared;

make -j4;
env DESTDIR=$DIR  make install;




    

ffmpeg_src;
ffmpeg_patch;






###############################################################################
# ffmpeg -- compile
###############################################################################
cd $DIR;
cd ffmpeg;
# NOTE: options are alphabetized, thankyouverymuch (makes comparison easier)
# NOTE: --enable-version3  is for prores decoding
# NOTE: libopenjpeg allows motion-JPEG jp2 variant

./configure $(echo "
--disable-shared
--disable-ffserver
--disable-vdpau

--enable-avfilter
--enable-gpl
--enable-libfaac
--enable-libmp3lame
--enable-libopencore-amrnb
--enable-libopencore-amrwb
--enable-libopenjpeg
--enable-libtheora
--enable-libvorbis
--enable-libxvid
--enable-nonfree
--enable-postproc
--enable-pthreads
--enable-static

--enable-ffplay
--enable-libvpx 
--enable-libx264
--enable-version3

--extra-cflags=-I${DIR?}/usr/local/include
--extra-ldflags=-L${DIR?}/usr/local/lib
--extra-cflags=-static

$MYCC
$MYCC2
$FFXTRA
");

#xxx --enable-libschroedinger # hmm stopped working in natty/oneiric ~oct2011...


    make -j4 V=1;
    make alltools;
    env DESTDIR=$DIR make install;
    if [ "$SHORTNAME" == "mac" ]; then
      sudo cp ffmpeg ffprobe tools/qt-faststart  /opt/local/bin/;
      if [ -x ffplay ]; then # fixxxme no ffplay still for Lion
          sudo cp ffplay /opt/local/bin/;
      fi
    else
      cp ffmpeg               $MYDIR/ffmpeg.$SHORTNAME;
      cp ffprobe              $MYDIR/ffprobe.$SHORTNAME;
      cp ffplay               $MYDIR/ffplay.$SHORTNAME;
      set -x;
      echo;echo;echo "NOTE: any changes to $MYDIR need to be committed..."
    fi
      

    



if [ "$SHORTNAME" == "mac" ]; then
    # now build mplayer from source (uses libx264 above and ffmpeg)

    echo "NOTE: on Mountain Lion OS you may not be able to ffplay playback since X11 is no longer installed by default"
    echo "NOTE: if so, see http://support.apple.com/kb/HT5293"

    cd $DIR;
    svn checkout svn://svn.mplayerhq.hu/mplayer/trunk mplayer;
    cd mplayer;
    mv ../ffmpeg .; # needed by mplayer -- will reconfig & remake it, sigh...

    # make it **not** recompile our ffmpeg (and later install bad libs) on us!!
    perl -i -p \
        -e 's/^\$\(FFMPEGLIBS\)\: \$\(FFMPEGFILES\) config\.h\n//;' \
        -e 's/^.*C ffmpeg.*\n//;' \
        Makefile;
    
    # worked finally!
    export LD_LIBRARY_PATH=/opt/local/lib:/usr/lib:/usr/local/lib:/lib;
    if [ "$OSTYPE" == "darwin11" ]; then
        perl -i -pe 's/\-mdynamic-no-pic //' configure;
    fi;

    # NOTE:  "disable-tremor" (seemed to be getting in way of vorbis)
    ./configure --prefix=/opt/local  $MYCC  --enable-menu  --enable-x264 --with-freetype-config=/opt/local/bin/freetype-config  --disable-tremor  --disable-ffmpeg_so  --extra-cflags="-I$DIR/usr/local/include -I/opt/local/include" --extra-ldflags="$DIR/usr/local/lib/libx264.a ";


    sudo chown -R $USER .; #should NOT have to do this, something screwy, fixit!
    make -j3;
    sudo make install;

    mv ffmpeg ..;

    echo "NOTE: on Mountain Lion OS you may not be able to ffplay playback since X11 is no longer installed by default"
    echo "NOTE: if so, see http://support.apple.com/kb/HT5293"

  ################################################################################
  #    unrelated ports packages that tracey likes/uses:
  # sudo port install lesspipe pcre wget ddrescue lftp spidermonkey ImageMagick avidemux
  # sudo port install p7zip unrar wine colordiff py-pygments lesspipe jp2a freetype
  # sudo port install gimp
  # sudo port install dvdauthor cdrtools dvdrw-tools
fi