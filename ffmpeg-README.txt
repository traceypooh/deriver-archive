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
--prefix=/usr/local
--extra-cflags=-I/usr/local/include
--extra-cflags=-I/usr/local/include/SDL
--extra-cflags=-I/opt/local/include

--extra-ldflags=-L/opt/local/lib
--extra-ldflags=-L/usr/local/lib
";

  DIR=/opt/local/x;
  DIRIN=$DIR/opt;

  if [ "$OSTYPE" != "darwin10.0" ]; then
    MYCC="--cc=clang"; # NOTE: esp. for mac lion!
    MYCC2="--disable-vda --enable-pic"; # NOTE: vda esp. for mac lion!  for mavericks, compile PIC since issue w/ making *static* build in ffmpeg otherwise!
  fi
 
  echo "step 1: install  macports -- see http://www.macports.org/install.php"

  # Some generally useful brew commands (in "terminal" app):
  #   brew doctor
  #   brew list
  #   brew search <pkg name>
  #   brew install <pkg name>

  brew install  lame theora libvorbis openjpeg faac freetype yasm opencore-amr xvid libvpx a52dec pkgconfig; # bzip2
  brew install  sdl; # xorg-libXfixes;   # X and SDL stuff for ffplay

else
  FFXTRA="
--prefix=/usr         
--enable-libfreetype
--enable-libgsm
--enable-libspeex
--extra-ldflags=-static
";


  # NOTE: (for libfreetype) (UGH SRSLY!?!) (July 2014)
  sudo perl -i -pe 's/lfreetype$/lfreetype -lpng12/'  /usr/lib/x86_64-linux-gnu/pkgconfig/freetype2.pc;

  
  
  SHORTNAME=$(lsb_release -cs); # eg: "precise"
  if [ "$SHORTNAME" != "natty"  -a  "$SHORTNAME" != "oneiric"  -a  "$SHORTNAME" != "precise"  -a  "$SHORTNAME" != "quantal"  -a  "$SHORTNAME" != "raring"  -a  "$SHORTNAME" != "trusty" ]; then
    echo "unsupported OS"; exit 1;
  fi

  # Make sure we have basic needed pkgs!
  sudo apt-get install yasm; # make sure we have an assembler!

  # http://packages.ubuntu.com/search?searchon=contents&arch=any&mode=exactfilename&suite=precise&keywords=libasound.a
  sudo apt-get -y install  \
    libschroedinger-dev  libdirac-dev  dirac \
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

  # we building most recent head of this below
  sudo apt-get -y purge libx264-dev;

fi;

function line(){ perl -e 'print "_"x80; print "\n\n";'; }


################################################################################
# ffmpeg, ffprobe, x264, mplayer
#       compile directly an ffmpeg that can create x264 and WebM
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
    for p in ffmpeg-aac.patch  ffmpeg-thumbnails.patch  ffmpeg-theora.patch  ffmpeg-PAT.patch; do
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
# 2nd line of disables is because it started Fing up ~May2013 and including
#   dlopen() ... dlclose() lines even though we dont want to allow shared...
if [ "${SHORTNAME?}" == "mac" ]; then
  PREFIX=/opt/local;
else
  PREFIX=/usr/local;
fi

./configure --enable-static --enable-pic --disable-asm \
--disable-avs --disable-opencl \
--extra-cflags=-I${DIRIN?}/local/include \
--extra-ldflags=-L${DIRIN?}/local/lib \
--prefix=$PREFIX;

make -j4;
sudo make install;

    

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
--enable-static

--enable-ffplay
--enable-libvpx 
--enable-libx264
--enable-version3

--extra-cflags=-I${DIR?}/usr/local/include
--extra-cflags=-static

$MYCC
$MYCC2
$FFXTRA
");



#xxx --enable-libschroedinger # hmm stopped working in natty/oneiric ~oct2011...

    make clean;
    make -j4 V=1;
    # make alltools; # no longer needed -- uncomment if you like
    env DESTDIR=${DIR?} make install;
    if [ "${SHORTNAME?}" == "mac" ]; then
      cp ffmpeg ffprobe  /usr/local/bin/;
      if [ -x ffplay ]; then # fixxxme no ffplay still for Lion
          cp ffplay /usr/local/bin/;
      fi
    else
      cp ffmpeg               $MYDIR/ffmpeg.$SHORTNAME;
      cp ffprobe              $MYDIR/ffprobe.$SHORTNAME;
      cp ffplay               $MYDIR/ffplay.$SHORTNAME;
      set -x;
      echo;echo;echo "NOTE: any changes to $MYDIR need to be committed..."
    fi
      

    



if [ "${SHORTNAME?}" == "mac" ]; then
    # now build mplayer from source (uses libx264 above and ffmpeg)

    cd ${DIR?};
    svn checkout svn://svn.mplayerhq.hu/mplayer/trunk mplayer;
    cd mplayer;
    mv ../ffmpeg .; # needed by mplayer -- will reconfig & remake it, sigh...
    

    # make it **not** recompile our ffmpeg (and later install bad libs) on us!!
    perl -i -p \
        -e 's/^\$\(FFMPEGLIBS\)\: \$\(FFMPEGFILES\) config\.h\n//;' \
        -e 's/^.*C ffmpeg.*\n//;' \
        Makefile;
    
    # worked finally!
    if [ $(echo "$OSTYPE"|cut -b1-6) = "darwin" ]; then
        perl -i -pe 's/\-mdynamic-no-pic //' configure;
    fi;

    # NOTE:  "disable-tremor" (seemed to be getting in way of vorbis)
    # NOTE:  needed order below ".. -lSDLMain -lopenjpeg .." to avoid libopenjpeg main from intercepting mplayer main (!!)
    ./configure --prefix=/usr/local  ${MYCC?}  --enable-menu  --enable-x264 --enable-theora --enable-liba52  --with-freetype-config=/usr/local/bin/freetype-config  --disable-tremor  --disable-ffmpeg_so  --extra-cflags="-I${DIR?}/x264 -I${DIR?}/usr/local/include -I/usr/local/include" --extra-ldflags="-L${DIR?}/x264" --extra-libs="-ltheoraenc -la52 -lx264 -llzma -lSDLMain -lopenjpeg";

    make -j3;
    make install;

    mv ffmpeg ..;


    ################################################################################
    #    unrelated brew packages that tracey likes/uses:
    # brew install lesspipe pcre wget ddrescue lftp spidermonkey avidemux exif coreutils pstree
    # brew install p7zip unrar colordiff jp2a freetype # py-pygments 
    # brew install imagemagick
    #
    # brew install wine
    # brew install gimp
    # brew install dvdauthor cdrtools dvdrw-tools
fi
