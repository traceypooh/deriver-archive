#!/bin/zsh -ex

###############################################################################
# author: Tracey Jaquith.   Software license AGPL version 3.
###############################################################################
# We compile our own ffmpeg because we have config options we want differently
# from the std ubuntu package AND because we have patches.
#
# For example, we want builtin mp3 creation (lame),
# AAC audio encoding and decoding support,
# and h.264/x264, x265 and ... and a pony 8-) ....
###############################################################################
DIR=/tmp/f
FFOS=bionic


export DEBIAN_FRONTEND=noninteractive
unset LD_LIBRARY_PATH # avoid any contamination when set!



function main() {
  if [ -e /p/ffmpeg-README.txt ]; then
    docker-setup
  fi

  MYDIR=$(d=`dirname $0`; echo $d | grep '^/' || echo `pwd`/$d)


  if [ ! -e /p/ffmpeg-README.txt ]; then
    docker-run
  fi


  config-setup
  dependencies-and-config
  openssl-static
  openjpeg-static
  ffmpeg-std
  x264
  x265
  ffmpeg-src
  ffmpeg-patch
  lib-fixes
  ffmpeg-compile
}



function docker-setup() {
  # FIRST, try to ensure as little pkg config warnings/carping as possible
  # NEXT, update any LTS to (a stable) LTS point release, eg:
  #   16.04  =>  16.04.1
  apt-get install -y apt-utils sudo
  apt-get upgrade -y

  # setup a minimally useful build env
  apt-get install -y  php-cli
  if [ -e /p/zshrc ]; then
    ln -sf /p/zshrc   /root/.zshrc
    ln -sf /p/aliases /root/.aliases
  fi
  export LC_ALL=C
}



function docker-run() {
  # linux, but _not_ docker
  if [ -e $MYDIR/../../docker/aliases ]; then
    cp $MYDIR/../../docker/{aliases,zshrc} $MYDIR/
  fi
  touch     $MYDIR/ffmpeg.next
  touch     $MYDIR/ffprobe.next
  touch     $MYDIR/ffmpeg.std
  touch     $MYDIR/ffprobe.std
  chmod 777 $MYDIR/ff*p*e*.next
  chmod 777 $MYDIR/ff*p*e*.std

  # NOTE: you stay (interactively) in the container if we fatal with an error - for debug/inspection
  sudo docker run -it --rm -v $MYDIR:/p ubuntu:$FFOS \
    bash -c "apt-get update  &&  apt-get install -y zsh  &&  (/p/ffmpeg-README.txt  ||  zsh)" \
    2>&1 |tee $MYDIR/flog

  rm -f $MYDIR/{aliases,zshrc}
  exit 0
}



function config-setup() {
  mkdir -m777 -p $DIR
  cd $DIR


  typeset -a CONFIG # array


  # NOTE: configure options are mostly alphabetized, thankyouverymuch (makes comparison easier)
  CONFIG+=(
    --enable-libmp3lame
    --enable-libfontconfig
    --enable-libfreetype
    --enable-libopencore-amrnb
    --enable-libopencore-amrwb
  )
  CONFIG+=(--enable-libopenjpeg) # allow motion-JPEG jp2 variant
  CONFIG+=(
    --enable-libopus
    --enable-libtheora
    --enable-libvorbis
    --enable-libvpx
    --enable-libx264
    --enable-libx265
    --enable-libxvid
    --enable-openssl

    --enable-avfilter
    --enable-gpl
    --enable-nonfree
  )
  CONFIG+=(--enable-version3) # for prores decoding
  CONFIG+=(
    --enable-static
    --disable-shared
    --disable-vdpau

    --extra-cflags=-static
  )
  # no statics (libs _they_ depend on) for these, so bleah excluded (retry each build...)
  # http://packages.ubuntu.com/search?searchon=contents&arch=any&mode=exactfilename&suite=wily&keywords=libass.a
  CONFIG_WANTED_NO_DICE+=(
    --enable-libass
    --enable-libbluray
  )
}


function dependencies-and-config() {
  # Make sure we have basic needed pkgs!
  perl -i -pe 's/^# deb-src/deb-src/' /etc/apt/sources.list  # ensure can 'apt-get source .. ' later
  apt-get update
  apt-get install -y curl autoconf cmake yasm git # make sure we have an assembler!

  apt-get -y install  \
    libbluray-dev \
    libopus-dev \
    libgsm1-dev \
    libmp3lame-dev \
    libspeex-dev \
    libopenjp2-7-dev \
    libopencore-amrnb-dev  libopencore-amrwb-dev \
    libvorbis-dev \
    libxvidcore-dev \
    libasound2-dev \
    libavfilter-dev \
    libtheora-dev \
    libvpx-dev \
    libnuma-dev \
    libfreetype6-dev \
    fontconfig expat \
    libass-dev \
    libsdl-dev  libsdl1.2-dev \
    libssh-dev openssl


  # we building most recent head of this _statically_ below
  apt-get -y purge libx264-dev libx265-dev



  CONFIG+=(
    --prefix=/usr
    --enable-libgsm
    --enable-libspeex
    --extra-ldflags=-static
    --pkg-config-flags=--static
  )
}



###############################################################################
# ffmpeg -- get source
###############################################################################
function ffmpeg-src() {
  if [ ! -e ffmpeg ]; then
    git clone git://source.ffmpeg.org/ffmpeg.git
  fi

  cd ffmpeg
  git reset --hard
  git clean -f
  git pull
  line
  git status
  line
  cd -
}



###############################################################################
# ffmpeg -- tracey patches
###############################################################################
#    -AAC audio (nondecreasing timestamps is fine; monotonic is too restrictive!)
#    -Better saved thumbnail names
#    -Better quality theora (like "ffmpeg2theora" tool; use both bitrate *and* qscale)
function ffmpeg-patch() {
  cd $DIR/ffmpeg
  PATDIR=
  for p in ffmpeg-aac.patch  ffmpeg-theora.patch  ffmpeg-PAT.patch  ffmpeg-thumbnails.patch; do
    if [ "$PATDIR" = "" ]; then
      # find the patches dir!
      if [ -e "$MYDIR/$p" ]; then
        PATDIR=file://$MYDIR
      elif [ -e "$MYDIR/../lib/ffmpeg/$p" ]; then
        PATDIR=file://$MYDIR/../lib/ffmpeg
      else
        PATDIR=http://archive.org/~tracey/downloads/patches
      fi
    fi

    curl "$PATDIR/$p" >| ../$p
    echo APPLYING PATCH $p
    patch -p1 < ../$p
  done
  cd -
}



###############################################################################
# ffmpeg -- do 1st pass default config and compile *and* install so
#           we can get modern libavutil, etc. installed in place for x264 build
###############################################################################
function ffmpeg-std() {
  ffmpeg-src
  ffmpeg-patch # test applying patches *first* in case any need updating

  ffmpeg-src   # revert any of the patches for this first clean/stock build
  cd $DIR/ffmpeg
  ./configure --prefix=/usr
  make -j6
  make install

  cp ffmpeg               $MYDIR/ffmpeg.std
  cp ffprobe              $MYDIR/ffprobe.std
  cd -
}



###############################################################################
# x264 -- make the lib we use to make h.264 video!
#         set it up so that we'll compile it in *statically*
#         now we can compile it and use most recent libavutil, etc.
#         (installed above with ffmpeg-std)
###############################################################################
function x264() {
  cd $DIR
  if [ ! -e x264 ]; then
    git clone git://git.videolan.org/x264.git
  fi
  cd x264
  git reset --hard
  git clean -f
  git pull
  git status

  # 2nd line of disables is because it started Fing up ~May2013 and including
  #   dlopen() ... dlclose() lines even though we dont want to allow shared...
  typeset -a XEXTRA # array
  XEXTRA+=(--prefix=/usr)
  XEXTRA+=(--disable-cli) # [10/2015..10/2017] x264 binary compiling being PITA so disabled...

  ./configure --enable-static --enable-pic --disable-asm --disable-avs --disable-opencl  $XEXTRA

  make -j6
  sudo make install
  cd -
}



function x265() {
  # need to make the static .a file!
  cd $DIR
  if [ ! -e x265 ]; then
    git clone https://anonscm.debian.org/git/pkg-multimedia/x265.git
  fi
  cd x265
  git reset --hard
  git clean -f
  git pull
  git status

  cd source
  cmake .
  make -j6
  make install
  cd $DIR
}



function openssl-static() {
  # bionic beta pkg didnt come with static .a lib file, so build from source xxx
  apt-get source openssl
  cd openssl*/
  ./config --prefix=/usr  &&  make -j4  &&  make install
  cd -
}



function openjpeg-static() {
  # bionic beta pkg didnt come with static .a lib file, so build from source xxx
  if [ ! -e openjpeg ]; then
    git clone https://github.com/uclouvain/openjpeg
  fi
  cd openjpeg
  ( mkdir -p build  &&  cd build  &&  cmake .. -DCMAKE_BUILD_TYPE=Release  &&  make  &&  make install )
  cd -
}



function lib-fixes() {
  # ugh! Oct2017 started having library linker lm and lpthread issues w/ config autodetection...
  perl -i -pe 's/ \-lmp3lame/ -lmp3lame -lm/' $DIR/ffmpeg/configure
  perl -i -pe 's/ \-lxvidcore/ -lxvidcore -lm -lpthread/' $DIR/ffmpeg/configure

  for pc in $(find /usr/local/lib/pkgconfig /usr/lib/x86_64-linux-gnu/pkgconfig |egrep 'x265.pc|fontconfig.pc|libopenjp2.pc|libssl.pc|libcrypto.pc'); do
    if ! $(fgrep Libs.private: $pc |fgrep -q lpthread); then
      perl -i -pe 's/Libs.private:/Libs.private: -lpthread/;' -e 's/ \-lgcc_s/ /g;' $pc
    fi
  done
}



function ffmpeg-compile() {
  cd $DIR
  cd ffmpeg
  ./configure $CONFIG


  make clean
  make -j6 V=1
  # make alltools; # no longer needed -- uncomment if you like
  env DESTDIR=${DIR?} make install
  cp ffmpeg               $MYDIR/ffmpeg.$SHORTNAME
  cp ffprobe              $MYDIR/ffprobe.$SHORTNAME
 #cp ffplay               $MYDIR/ffplay.$SHORTNAME
  set -x
  echo
  echo
  echo "NOTE: any changes to $MYDIR need to be committed..."
}


function line(){
  perl -e 'print "_"x80; print "\n\n"';
}



main
