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
DIR=/f


export DEBIAN_FRONTEND=noninteractive
unset LD_LIBRARY_PATH # avoid any contamination when set!

MYDIR=${0:a:h}
ME=$(basename $0)

typeset -a CONFIG # array


function main() {
  [ -e /p/$ME ]  ||  docker-run
  [ -e /p/$ME ]  ||  exit 0

  docker-setup
  config-setup
  dependencies-and-config
  aac-setup
  openssl-static
  openjpeg-static
  vorbis-static
  ffmpeg-std
  x264
  x265
  ffmpeg-src
  ffmpeg-patch
  lib-fixes
  ffmpeg-compile
}


function docker-run() {
  # linux, but _not_ docker
  [ -e $MYDIR/../../docker/aliases ]  &&  cp $MYDIR/../../docker/{aliases,zshrc} $MYDIR/

  touch -a  $MYDIR/ffmpeg.next
  touch -a  $MYDIR/ffprobe.next
  touch -a  $MYDIR/ffmpeg.std
  touch -a  $MYDIR/ffprobe.std
  chmod 777 $MYDIR/ff*p*e*.next
  chmod 777 $MYDIR/ff*p*e*.std

  # NOTE: you stay (interactively) in the container if we fatal with an error - for debug/inspection
  sudo docker run -it --rm -v $MYDIR:/p -v $MYDIR/../lib/ffmpeg:/pat ubuntu:rolling \
    bash -c "apt-get update  &&  apt-get install -y zsh  &&  (/p/$ME  ||  zsh)" \
    2>&1 |tee $MYDIR/flog

  rm -f $MYDIR/{aliases,zshrc}
}


function docker-setup() {
  # FIRST, try to ensure as little pkg config warnings/carping as possible
  # NEXT, update any LTS to (a stable) LTS point release, eg:
  #   16.04  =>  16.04.1
  apt-get install -y apt-utils sudo
  apt-get upgrade -y

  # setup a minimally useful build env
  apt-get install -y  php-cli
  [ -e /p/zshrc   ]  &&  ln -sf /p/zshrc   /root/.zshrc
  [ -e /p/aliases ]  &&  ln -sf /p/aliases /root/.aliases

  export LC_ALL=C
}


function config-setup() {
  mkdir -m777 -p $DIR
  cd $DIR


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
  CONFIG_WANTED_xxx+=(
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


  # we build most recent head of this _statically_ below
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
  [ -e ffmpeg ]  ||  git clone git://source.ffmpeg.org/ffmpeg

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
function ffmpeg-patch() {
  cd ffmpeg
  for p in /pat/ffmpeg-aac.patch  /pat/ffmpeg-thumbnails.patch; do
    echo APPLYING PATCH $p
    patch -p1 < $p
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
  cd ffmpeg
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
  [ -e x264 ]  ||  git clone https://code.videolan.org/videolan/x264

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
  [ -e x265 ]  ||  git clone https://anonscm.debian.org/git/pkg-multimedia/x265

  cd x265
  git reset --hard
  git clean -f
  git pull
  git status
  cd -

  cd x265/source
  cmake .
  make -j6
  make install
  cd -
}


function openssl-static() {
  # bionic beta pkg didnt come with static .a lib file, so build from source xxx
  apt-get source openssl
  cd openssl*/
  ./config --prefix=/usr  &&  make -j4  &&  make install
  cd -
}


function aac-setup() {
  # for highest quality AAC encoding.  pkgs only come with .so file(s), so build .a from source
  apt-get -y install  libtool  dpkg-dev
  apt-get source libfdk-aac-dev

  cd fdk-aac*/
  ./autogen.sh
  ./configure --enable-static --disable-shared --prefix=/usr  &&  make -j4  &&  make install
  cd -

  CONFIG+=(--enable-libfdk-aac)
}

function openjpeg-static() {
  # bionic beta pkg didnt come with static .a lib file, so build from source xxx
  [ -e openjpeg ]  ||  git clone https://github.com/uclouvain/openjpeg

  cd openjpeg
  ( mkdir -p build  &&  cd build  &&  cmake .. -DCMAKE_BUILD_TYPE=Release  &&  make  &&  make install )
  cd -
}

function vorbis-static() {
  # focal ubuntu started having issues w/ the .a file compiling in..
  apt-get source  libvorbis-dev

  cd libvorbis*/
  PKG_CONFIG=/usr/bin/pkg-config ./configure --enable-static --disable-shared --prefix=/usr
  make -j4  &&  make install
  cd -
}

function lib-fixes() {
  # ugh! Oct2017 started having library linker lm and lpthread issues w/ config autodetection...
  perl -i -pe 's/ \-lmp3lame/ -lmp3lame -lm/'             ffmpeg/configure
  perl -i -pe 's/ \-lxvidcore/ -lxvidcore -lm -lpthread/' ffmpeg/configure

  for pc in $(find /usr/local/lib/pkgconfig /usr/lib/x86_64-linux-gnu/pkgconfig |egrep 'x265.pc|fontconfig.pc|libopenjp2.pc|libssl.pc|libcrypto.pc'); do
    ( fgrep Libs.private: $pc |fgrep -q lpthread
    )  ||  perl -i -pe 's/Libs.private:/Libs.private: -lpthread/;' -e 's/ \-lgcc_s/ /g;' $pc
  done
}


function ffmpeg-compile() {
  cd ffmpeg
  ./configure $CONFIG


  make clean
  make -j6 V=1
  # make alltools; # no longer needed -- uncomment if you like
  env DESTDIR=${DIR?} make install
  cp ffmpeg               $MYDIR/ffmpeg.next
  cp ffprobe              $MYDIR/ffprobe.next
 #cp ffplay               $MYDIR/ffplay.next
  cd -

  set -x
  echo
  echo
  echo "NOTE: any changes to $MYDIR need to be committed..."
}


function line(){
  perl -e 'print "_"x80; print "\n\n"';
}


main
