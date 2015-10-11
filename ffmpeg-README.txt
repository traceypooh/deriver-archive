#!/bin/zsh -ex

###############################################################################
# author: Tracey Jaquith.  Software license GPL version 2.
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
# eg: "trusty" for ubuntu; or "mac"
SHORTNAME=$(lsb_release -cs 2>/dev/null  ||  echo mac);
if [ "$SHORTNAME" != "mac"  -a  "$SHORTNAME" != "precise"  -a  "$SHORTNAME" != "trusty"  -a  "$SHORTNAME" != "wily" ]; then
  echo "unsupported OS"; exit 1;
fi



unset LD_LIBRARY_PATH; #avoid any contamination when set!
MYDIR=$(d=`dirname $0`; echo $d | grep '^/' || echo `pwd`/$d);


sudo mkdir -p $DIR;
sudo chown $USER $DIR;
cd $DIR;




MYCC="";
typeset -a CONFIG; # array


# NOTE: configure options are mostly alphabetized, thankyouverymuch (makes comparison easier)
# NOTE: --enable-version3  is for prores decoding
# NOTE: libopenjpeg allows motion-JPEG jp2 variant
CONFIG+=(
--enable-libfaac 
--enable-libmp3lame 
--enable-libfontconfig 
--enable-libfreetype
--enable-libopencore-amrnb 
--enable-libopencore-amrwb 
--enable-libopenjpeg 
--enable-libopus
--enable-libtheora 
--enable-libvorbis 
--enable-libvpx  
--enable-libx264 
--enable-libx265 
--enable-libxvid 

--enable-avfilter
--enable-gpl
--enable-nonfree 
--enable-version3 
--enable-static 
--disable-shared
--disable-ffserver
--disable-vdpau

--extra-cflags=-static
--extra-cflags=-I${DIR?}/usr/local/include
);
# no statics (super) for these (or direct dependencies, eg: ass) so that sux (retry each build...)
# http://packages.ubuntu.com/search?searchon=contents&arch=any&mode=exactfilename&suite=wily&keywords=libass.a
RETRY+=(
--enable-libass
--enable-libbluray
);



if [ $(uname -s) = 'Darwin' ]; then
  MYCC="--cc=clang"; # NOTE: esp. for mac lion+!
  CONFIG+=($MYCC);
  
  CONFIG+=(--disable-vda); # esp. for mac lion+!  
  CONFIG+=(--enable-pic);  # for mavericks+, compile PIC since issue w/ making *static* build in ffmpeg otherwise!
 
  CONFIG+=(
--enable-sdl
--enable-ffplay 

--prefix=/usr/local
--extra-cflags=-I/usr/local/include
--extra-cflags=-I/usr/local/include/SDL
--extra-cflags=-I/opt/local/include

--extra-ldflags=-L/opt/local/lib
--extra-ldflags=-L/usr/local/lib
);             


  DIR=/opt/local/x;
  DIRIN=$DIR/opt;

  echo "step 1: install  macports -- see http://www.macports.org/install.php"

  # Some generally useful brew commands (in "terminal" app):
  #   brew doctor
  #   brew list
  #   brew search <pkg name>
  #   brew install <pkg name>

  brew install  lame theora libvorbis openjpeg faac freetype yasm opencore-amr xvid libvpx a52dec pkgconfig opus-tools x265; # bzip2
  brew install  sdl; # SDL stuff for ffplay

else
  # Make sure we have basic needed pkgs!
  sudo apt-get install yasm; # make sure we have an assembler!

  sudo apt-get -y install  \
    libbluray-dev \
    libopus-dev \
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
    libtheora-dev \
    libvpx-dev \
    libx265-dev  libnuma-dev \
    libfreetype6-dev \
    fontconfig expat \
    libass-dev \
    libsdl-dev  libsdl1.2-dev \
  ;

  # we building most recent head of this below
  sudo apt-get -y purge libx264-dev;



  CONFIG+=(
--prefix=/usr         
--enable-libgsm
--enable-libspeex
--extra-ldflags=-static
);

  # helps with finding crazily located libfreetype (and more)
  LGNU=/usr/lib/x86_64-linux-gnu;
  CONFIG+=(--pkg-config=/usr/bin/pkg-config --pkg-config-flags=--static);
  
  # ugh, horrid, libopenjpeg is facepalm again (no .a distributed in wily) xxx
  set +e;
  JPA=$(dirname $(find $DIR -name libopenjpeg.a)); # where liboppenjpeg.so lives
  set -e;
  if [ "$JPA" = "" ]; then
    cd $DIR;
    apt-get source libopenjpeg-dev;
    cd openjpeg*/;
    ./bootstrap.sh;
    ./configure;
    make -j4;
    cd $DIR;
  fi;
  
  JPA=$(dirname $(find $DIR -name libopenjpeg.a)); # where liboppenjpeg.so lives
  CONFIG+=(--extra-ldflags=-L$JPA --extra-ldflags=-L$LGNU --extra-ldflags=-lfreetype);
fi;


function line(){ perl -e 'print "_"x80; print "\n\n";'; }


################################################################################
# ffmpeg, ffprobe, x264, mplayer
#       compile directly an ffmpeg that can create x264 and WebM
################################################################################







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
    if [ "$PATDIR" = "" ]; then
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
if [ "${SHORTNAME?}" = "mac" ]; then
  XEXTRA="--prefix=/opt/local";
else
  XEXTRA="--disable-cli"; # jul15, 2015 x264 binary compiling being PITA so disabled...
fi

./configure --enable-static --enable-pic --disable-asm \
--disable-avs --disable-opencl \
--extra-cflags=-I${DIRIN?}/local/include \
--extra-ldflags=-L${DIRIN?}/local/lib \
$XEXTRA;

make -j4;
sudo make install;

    

ffmpeg_src;
ffmpeg_patch;






###############################################################################
# ffmpeg -- compile
###############################################################################
cd $DIR;
cd ffmpeg;
./configure $CONFIG;


make clean;
make -j4 V=1;
# make alltools; # no longer needed -- uncomment if you like
env DESTDIR=${DIR?} make install;
if [ "${SHORTNAME?}" = "mac" ]; then
  cp ffmpeg ffprobe ffplay  /usr/local/bin/;
else
  cp ffmpeg               $MYDIR/ffmpeg.$SHORTNAME;
  cp ffprobe              $MYDIR/ffprobe.$SHORTNAME;
 #cp ffplay               $MYDIR/ffplay.$SHORTNAME;
  set -x;
  echo;echo;echo "NOTE: any changes to $MYDIR need to be committed..."
fi
      

    



if [ "${SHORTNAME?}" = "mac" ]; then
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
  perl -i -pe 's/\-mdynamic-no-pic //' configure;
  
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
