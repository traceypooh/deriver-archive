<?

class Video {
  use Assert;


  // **hopefully** the near ultimate fix for MPEG-TS A/V sync drifting!
  // Make sure we use '-vsync' param.
  // This came about from last weeks 'new' MSNBCW_2012121* TV programs that were in HD with 5.1 audio,
  // but recently started arriving with local affiliate  SD commercials in stero audio.
  // Those items became totally borked or sometimes just failed rest of .mp4 after that commercial set
  // until came up with this new A/V PTS/DTS sync strategy!
  //
  // A particularly helpful part in this was:
  //   https://ffmpeg.org/trac/ffmpeg/ticket/187#comment:6
  //
  // Which basically advocates, pseudocode:
  //   $vsync = ( wmv ? 1 : 0 )
  //   if you get a "non monotonic timestamp" error, change $vsync to 1 (if it wasn't 1 already) and add '-r' arg
  // however that made massive A/V slip in many shows in jwplayer **flash plugin** (only), so
  // went instead with (just) "-vsync 1", and add '-r' arg secondarily if the 1st try fails

  const AV_ANTI_DRIFT = ' -vsync 1 ';//flash works - w/ some expense to dupe/nix video frames to meet framerate
  // dead bodies:
  //const AV_ANTI_DRIFT='-vsync 0';             //flash plugin has A/V drift
  //const AV_ANTI_DRIFT='-vsync 0 -copyts';     //flash plugin has A/V drift
  //const AV_ANTI_DRIFT='-vsync 0 -r xxx';      //flash plugin has A/V drift
  //const AV_ANTI_DRIFT='-vsync 0 -fflags +genpts+igndts';  // [ditto above]
  //const AV_ANTI_DRIFT='-async 1';             //only currects the *start* per dox!
  //const AV_ANTI_DRIFT='-af aresample=8000';   //flail, works for small test cases; not enough testing; bad science

  // Our prior 2012 way...
  // in addition to the 5.1/stereo commercials that "killed this" set of prior args (above),
  // this item (with Mar2012 ffmpeg):
  //   /details/MSNBCW_20110817_130000_The_Daily_Rundown
  // had mp4 that locked up and died midway through (fixed now with Dec2012 ffmpeg + "-vsync 1")
  //const AV_ANTI_DRIFT='-muxdelay 5 -copyts -async 88200 -adrift_threshold 5 -dts_delta_threshold 5';


  // Also worth exploring in future if we have other A/V sync issues...  from Stefano Sabatini Jan2013
  // What I used when I need the same functionality:
  // min_comp=0.001:min_hard_comp=0.1
  //    Exegesis:
  // delta = input PTS - expected PTS
  // min_comp=d
  // basically enables soft/hard compensation in case delta >= d.
  // Compensation consists in applying one of these:
  //   stretching/squeezing (soft compensation)
  //   filling or trimming (hard compensation)
  // Since you want to enable *filling*/padding in this case,
  // min_hard_comp=0.1 will add samples if delta >= 0.1 (this happens in
  // case of big delay at the beginning of the file), soft compensation in
  // case of 'small' (<0.1 seconds) gaps. Since min_hard_comp=0.1 is the
  // default, you can simply set equivalently:
  // aresample=min_comp=0.001
  // See ffmpeg-resampler(1)


  // make sure we don't make TOO MANY thumbs/small files for given item!
  const MAX_THUMBS_PER_ITEM = 300;


  public static function ffmpegV0($file = null) {
    return Video::ffmpeg($file, '-v 0');
  }

  public static function ffmpeg($file = null, $args1 = '') {
    // for probing for codec info/characteristics, up from ~5MB to ~200MB to analyze.
    // for duration, check first 900 seconds (arg is in microseconds), not default 5 seconds.
    $ffmpeg = Config::petaboxPath('bin-ffmpeg');
    $args = (defined('LIMITLESS') ? ' -threads 16 ' : ' -threads 2 ').(Video::file_is_tv($file) ? ' -f mpegts ' : '');

    return "$ffmpeg $args1 -analyzeduration 900000000000 -probesize 200M $args";
  }


  // given a short or fully qualified filename (no CGI args!),
  // tries to determine if it *looks like* it's TV, pattern-wise
  // and returns true/false
  public static function file_is_tv($file = null) {
    return ($file !== null  &&  preg_match('=([^/]+)\.mpg$=i', $file, $mat)  &&  TV::is_tv($mat[1]));
  }






  // sanity check the overall identified length/duration of videos, adjusting as needed
  private static function adjust_length($file, &$arr, $tvarchive, $use_packets_length = false) {
    // sanity check the overall bytes/sec!  (see if the 'length' is waaaaay off)
    // blu-ray is unlikely to ever max > 64 Mbit/s video, for example...
    // ( 64 Mbit/s == 66536 Kbit/s == 67108864 bit/s )
    $kbs = 0;
    if ($arr['bytes'] > 0  &&  ($arr['length'] == 0  ||  ($arr['length'] != 0  &&
                                                      ($kbs = ($arr['bytes'] / $arr['length'] / 1024 * 8)) &&
                                                      ($kbs < 10  ||  $kbs > 100000)))) {
      $kbs = round($kbs, 1);
      error_log("Duration of {$arr['length']} seconds (=> ~$kbs kb/s) seems wrong...");

      if (!$arr['length']  &&  preg_match('/\.(shn(f){0,1}|mkw)$/i', $file)) {
        // shorten files are pretty awful and need a special tool just to get duration
        $shntool = Config::petaboxPath('bin-shntool');
        // YES! this PATH thing is goofy cray.  welcome to opensource s/w ladies and gentlemens.
        // sometime we'll have a drink together and I'll tell you how many hours that took to find..
        $mylen = Util::cmd('env PATH=' . dirname($shntool) . " $shntool len " . Util::esc($file) .
          " |tail -n +2 - |egrep -o '[^ ]+' |head -1");
        $mylen = round(Video::hms2sec($mylen), 1);
        if (is_numeric($mylen)  &&  $mylen > 0) {
          error_log("Using alternate method of $mylen seconds (=> ~$kbs kb/s)!");
          $arr['length'] = $mylen;
          return;
        }
      }

      $mylen = TV::alt_duration($file);
      $arr['length_vpackets'] = $mylen;
      if ($mylen > 10  &&
          ($kbs = ($arr['bytes'] / $mylen / 1024 * 8))  &&
          ($kbs >= 10  ||  $kbs <= 100000)) {
        // Seems like we have a more accurate length.
        $kbs = round($kbs, 1);
        error_log("Using alternate method of $mylen seconds (=> ~$kbs kb/s)!");
        $arr['length'] = $mylen;
        return;
      }
    }
    //error_log("$kbs kb/s");


    if (!$tvarchive)
      return;



    if (!$arr['length']  ||  $use_packets_length) {
      if (!$arr['length'])
        echo 'Duration not detected';
      $arr['length'] = $arr['length_vpackets'] = TV::alt_duration($file);
      return;
    }

    // TV item -- load its recording .xml to determine the 'expected' duration
    $dir = dirname(realpath($file));
    $id = basename($dir);
    if (!($xml = @simplexml_load_file("{$dir}/{$id}.xml")))
      return;

    if (($sec = $arr['length'])  &&
        isset($xml->DURATION)  &&
        count($xml->DURATION)  &&
        (preg_match('/^(\d+):(\d+):(\d+)$/', $xml->DURATION[0], $mat))  &&
        ($duration = $mat[1] * 3600 + $mat[2] * 60 + $mat[3])) {
      $toolong  = ($sec - $duration > TV::ALLOWED_LONGER);
      $tooshort = ($duration - $sec > TV::ALLOWED_SHORTER);

      if ($toolong  ||  $tooshort) {
        // We have determined that the "ffmpeg peeked" duration is (10+ min)
        // longer than what the recorder thought it recorded!
        //
        // OK this is wacky but bear with me, please!
        // Some videos can drop A/V packets at various points during recording.
        // Some videos can "roll the counter" of the pts/dts (program and display
        // timestamps).  ffmpeg doesn't like the "roll the counter"!
        // So let's do an alternate and more expensive duration check now.
        error_log("Duration of $sec seconds seems wrong (compared to recorder showing $duration seconds)");
        $mylen = $arr['length_vpackets'] = TV::alt_duration($file);
        $use_alt = false;

        if ($toolong  &&  ($mylen <= $duration  ||  ($mylen - $duration) <= TV::ALLOWED_LONGER))
          $use_alt = true;

        if ($tooshort  &&  ((($duration - $mylen) <= TV::ALLOWED_SHORTER) ||
                            // or if 95% 'intended' duration -- go with it!!
                            ($duration > $mylen  &&  ($mylen / $duration >= 0.95)))) {
          $use_alt = true;
        }

        if ($use_alt) {
          // Seems like we have a more accurate length.
          // Let's use it for all our _meta.xml purposes, etc.
          error_log('Using alternate method!');
          $arr['length'] = $mylen;
        }
      }
    }
  }



  // returns basic A/V info for given file in array, using "ffmpeg -i".
  //
  // runs a ffmpeg cmd to identify the video/audio parameters of a file
  // and returns an array of key/val params, eg:
  // (
  //  [width]   => 640
  //  [height]  => 480
  //  [fps]     => 20      (may be fractional, may be like '48k')
  //  [length]  => 2.75
  //  [audio]   => true
  //  [ar]      => 44100
  //  [ac]      => aac
  //  [vc]      => h264
  // )
  public static function id($file = '') {
    $r = Video::identify($file ? $file : current(glob('*.{mp4,mpeg4,m4v,mov,avi,mpg}', GLOB_BRACE)), true, 500);
    printr($r);
    return $r;
  }
  public static function identify(
    $file,
    $print = false,
    $tail = 2000,
    $tvarchive = false,
    $use_packets_length = false
  ) {
    // kill if hangs after 30 sec!
    // we give it this long since some crazy *NON* interleaved AVI files
    // can take nearly 20+ seconds to inspect...

    // NOTE: we ignore the exit status of this command because it *will*
    // fail with a "Must supply at least one output file" message.
    // Give it (only) a short time to run (everyonce in awhile it repeat logs
    // an error message, which runs deriver proc out of memory)
    // only keep the first 150 lines (in case it went crazy logging)
    $lines = [];
    $retval = null;
    exec(Util::timeout(600) . ' ' . Video::ffmpeg($file) . ' -an -vn -i ' . Util::esc($file) .
         ' 2>&1 | ' . Util::timeout(600) . " tail -$tail", $lines, $retval);

    if ($print) {
      echo "  ##################  CLIP INFO  ################ \n  # ".
        join("\n  # ", $lines).
        "\n  ##################  CLIP INFO  ################ \n";
    }

    $out = join("\n", $lines);




    $width  = null;
    $height = null;
    $video = '';
    $vline = self::pick_vstream($file, $out);
    if (preg_match('/^(Stream .*? Video: .*?, (\d+)x(\d+)(, | \[)(.*))/', $vline, $mat)) {
      $width = $mat[2];
      $height = $mat[3];
      // save this for later -- we gonna parse other params from it
      // wacky items can have 2+ video streams in them!  so we pick the
      // first one that has parseable width/height
      // eg: /details/FOXNEWS_20091125_020000_Hannity
      // has 2 Video streams, and the 1st one is junky, 2nd one we want!
      $video = $mat[1];
    }
    //error_log("VIDEO VAR: $vline");



    // compute length of movie in seconds (may have decimal component)
    // compute frames per second of movie (may have decimal component)
    $arr = [
      'width'   => $width,
      'height'  => $height,
      'fps'     => null,
      'audio'   => 'false',
      'bytes'   => (strncmp($file, 'http://', 7) == 0 ? 0 : @filesize($file)),
      'length' => (preg_match(
                    '/\n *Duration: (\d+):(\d+):([\d\.]+)/',
                    $out,
                    $mat
                  ) ? ((60 * 60 * $mat[1]) + (60 * $mat[2]) + $mat[3]) : null),

      // example (wily+):
      //   built with gcc 5.2.1 (Ubuntu 5.2.1-21ubuntu2) 20151003
      'ffmpeg-version' => (preg_match('/\n  built with .*? \([^\)]+\) (\d{8})\n/', $out, $mat) ? $mat[1] : ''),
    ];
    if ($arr['ffmpeg-version'] == '20151010')
      $arr['ffmpeg-version']   = '20160116'; // wack! took *gcc* build time?!




    Video::adjust_length($file, $arr, $tvarchive, $use_packets_length);


    if (strpos($out, 'multiple edit list entries, a/v desync might occur, patch welcome') > 0)
      $arr['vsync'] = 1;


    // For TV archive items, we want to always use the first audio track found
    // Save 2 streams like: "0.1,0.2"
    if (preg_match_all('/ Stream \#(\d+.\d+).*?(: Audio: .*)/', $out, $alines)) {
      // should we prioritize 5.1 channels first?  yes, unless like TV but *not* HD
      $mpegts = (strpos($out, 'Input #0, mpegts, from ') !== FALSE);
      $HDTV = ($mpegts  &&  $arr['height'] >= 720);
      $prefer51 = (!$mpegts || $HDTV);

      if ($tvarchive  &&  !$prefer51) {
        // uho!  KNTV started uploading SD material with *only* good audio in *5.1*!  (feb2014)
        // eg: /details/KNTV_20140211_123000_Today_in_the_Bay
        // So did KGO starting about here: /details/KGO_20140710_030000_The_Middle
        list($begTS, $ch) = Util::nofatal(function () use ($file) {
          return TV::identifierTS2(basename($file));
        });
        if (($ch == 'KNTV'  &&  $begTS >= 1391212800)  || // strtotime('2014-02-01')
            ($ch == 'KGO'   &&  $begTS >= 1404961200)  || // strtotime('2014-07-10')
            ($begTS >= 1513296000  &&   // strtotime('2017-12-15')
             in_array($ch, ['BETW', 'CNBC', 'FBC', 'FOXNEWSW', 'KGO', 'KPIX', 'KQED', 'KQEH', 'MSNBCW',
                            'WJLA', 'WRC', 'WTTG', 'WUSA']))
        ) {
          $prefer51 = true;
        }
      }



      //printr($alines);
      $arr['astreams'] = '';
      $arr['audio_nchannels'] = 0;
      $eng  = []; // prefer eng "5.1" when HD, eg: /details/KQED_20120625_210000_Rick_Steves_Europe
      $hd   = []; // ..then prefer "5.1" when HD, eg: /details/KNTV_20120625_210000_We_the_People_With_Gloria_Allred
      $best = []; // ..then prefer stereo then mono
      $rest = []; // ..and o/w take any riff-raff if need be...
      for ($lineN = 0; $lineN < count($alines[0]); $lineN++) {
        $aline    = $alines[0][$lineN];
        $streamID = $alines[1][$lineN];

        // oneiric era + goes from/to:
        // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
        // Audio: mp1 ([6][0][0][0] / 0x0006), 48000 Hz, stereo, s16, 416 kb/s
        $aline = preg_replace('= \(\[\d+\]\[\d+\]\[\d+\]\[\d+\] / 0x\d+\)=', '', $aline);

        if (count($alines[0]) == 3  &&  $tvarchive) {
          // throw out likely misdetected MP3 stereo lines (used to be wackier (below) until trusty...)
          if (preg_match('=: Audio: mp3, .*?, stereo, =', $aline))
            continue;
        }


        if (count($alines[0]) == 3  &&
            // throw out suspected-decoded-wrong audio streams like:
            // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
            // /details/WUSA_20110519_223000_CBS_Evening_News_With_Katie_Couric
            (strpos($aline, ': Audio: mp1,') > 0  ||
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091019_160000_9News_Now_at_Noon
             strpos($aline, ': Audio: aac, 24000 Hz, 7.1,') > 0  ||  //jan2014 version
             strpos($aline, ': Audio: aac, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)') > 0  || //sep2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)') > 0  || //mar2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 7.1(wide)') > 0  || //nov2011 version
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091118_140000_9News_Now_at_9am    -- it should be noted this item is botched/glitchy
             strpos($aline, ': Audio: aac, 7.1, fltp, ') > 0  || //jan2014 version
             strpos($aline, ': Audio: aac, 3.0, fltp, ') > 0  || //dec2012 version
             strpos($aline, ': Audio: aac, 3.0, s16, ') > 0  || //sep2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)') > 0  || //mar2012 version
             strpos($aline, ': Audio: aac, 7350 Hz, 3 channels') > 0 )) { /*nov2011 version*/
          continue;
        }

        if ($prefer51  &&  preg_match('$\]\(eng\): Audio: .*?, (5\.1)(\(side\))*, $', $aline, $m)) {
          // prioritize 'eng'(lish) 5.1 channels
          $eng[] = $streamID;
          $arr['audio_nchannels'] += floatval($m[1]);
        } elseif ($prefer51  &&  preg_match('$: Audio: .*?, (5\.1)(\(side\))*, $', $aline, $m)) {
          // prioritize 5.1 channels next
          $hd[] = $streamID;
          $arr['audio_nchannels'] += floatval($m[1]);
        } elseif (preg_match('/, (\d+) channels, /', $aline, $m)) {
          if ($m[1] == 0) {
            // throw out audio tracks w/ 0-channels
            // eg on natty, /details/WUSA_20091225_000000_9News_Now_Tonight
            continue;
          } elseif ($m[1] <= 2) {
            $best[] = $streamID; // mono/stereo track.  we likey!
          } else {
            $rest[] = $streamID; // surround-ish track.  2nd choice..
          }

          $arr['audio_nchannels'] += $m[1];
        } elseif (preg_match('/, stereo, /', $aline, $m)) {
          $arr['audio_nchannels'] += 2;
          $best[] = $streamID;
        } elseif (preg_match('/, mono, /',   $aline, $m)) {
          $arr['audio_nchannels'] += 1;
          $best[] = $streamID;
        } elseif (preg_match('$: Audio: .*?, ([\d\.]+) Hz, ([\d\.]+)$', $aline, $m)) {
          // rare!  only started seeing w/ 2020 ffmpeg build
          // egs: 32000 Hz, 4.0        - /details/TestDvRip/DV_Test.mov
          //      48000 Hz, 5.0(side)  -
          // https://archive.org/details/WJLA_20160415_100000_Good_Morning_Washington__600#start/1080/end/1140
          //   extracted test here:  https://archive.org/~tracey/_/robin/
          $arr['ar'] = $m[1];
          $arr['audio_nchannels'] += $m[2];
          $best[] = $streamID;
        } else {
          $rest[] = $streamID;
        }

        if ($arr['audio_nchannels'])
          $arr['audio'] = 'true';

        if (!isset($arr['ac'])  &&  preg_match('/: Audio: ([^, ]+)/', $aline, $mm))
          $arr['ac'] = $mm[1];

        // Some source MP3s make ffmpeg fail to output a WAVE unless we parse and
        // pass on the audio sample rate, eg: /details/BananaRepublicING
        if (!isset($arr['ar'])  &&  preg_match('/: Audio: .*?, ([\d\.]+) Hz/', $aline, $mm))
          $arr['ar'] = $mm[1];
      }

      // basically make comma-separated string of: $eng,$hd,$best,$rest
      $eng  = array_flip($eng);
      $hd   = array_flip($hd);
      $best = array_flip($best);
      $rest = array_flip($rest);
      $arr['astreams'] = join(',', array_keys($eng + $hd + $best + $rest));
    }



    if (preg_match('/(Stream \#(\d+.\d+).*?: Video: )/', $video, $m)) {
      $vline = $m[1];
      $arr['vstreams'] = $m[2];

      // ..and save any program ID that video stream has!
      foreach (explode("\n  Program ", $out) as $chunk) {
        if (preg_match('@^(\d+)$@m', $chunk, $m)  &&  preg_match('@^\s+Stream #@m', $chunk)  &&
            strpos($chunk, $vline)) {
          $arr['program'] = $m[1];
          break;
        }
      }
    }


    // Set the framerate
    if (preg_match('/Stream .*? Video: (.*)/', $video, $m)) {
      $tmp = Video::fps($file, $m[1], $arr['vstreams']);
      if (is_numeric($tmp)) {
        if ($tmp > 0)
          $arr['fps'] = $tmp;
      } elseif ($tmp !== '') {
        $fps = Video::tofloat($tmp);
        // true story for real! /details/OvercomePt.3-OvercomeTheWorldWithLove_787
        // has frame rate of '0'
        if ($fps != 0)
          $arr['fps'] = $fps;
      }
    }




    // save aspect-related information
    if (preg_match('/Stream .*? Video: .*?(\[|, )[SP]AR (\d+:\d+)/', $video, $m))
      $arr['PAR'] = $m[2];
    if (preg_match('/Stream .*? Video: .*?(\[|, )[SP]AR (\d+:\d+) DAR (\d+:\d+)/', $video, $m))
      $arr['DAR'] = $m[3];



    // save the Audio and Video Codecs and other info used for video..
    if (preg_match('/: Video: ([^, ]+)/', $video, $m))       $arr['vc' ] = $m[1];

    if (preg_match('/: Video: [^,]+, ([^,]+),/', $video, $m))
      $arr['pix'] = strtok($m[1], '('); // ... and nix any ending like "yuv420p(tv, bt709)" ( /details/pews )



    // now see if there is a rotation exif tag, like some iphone4+ videos
    // 180 degree flipped example:  /details/Jazz_Alley_in_Fort_Collins
    // 4GB+ rotated video eg: /details/DonKnuthOnTypographyAtColophonClubMarch122013
    if (!$tvarchive  &&  preg_match('/^\s+rotate\s+:\s+(90|180|270)\s*/m', $out, $m)) {
      $arr['rotation'] = $m[1];
      if ($arr['rotation'] == 90  ||  $arr['rotation'] == 270) {
        // swap width and height
        $tmp = $arr['width'];
        $arr['width'] = $arr['height'];
        $arr['height'] = $tmp;
      }
    }


    return $arr;
  }


  public static function identify2($file, $iso = '') {
    $mplayer = Config::value('bin-mplayer');

    $lines = [];

    // kill if hangs after 30 sec!
    // we give it this long since some crazy *NON* interleaved AVI files
    // can take nearly 20+ seconds to inspect...

    // NOTE: we ignore the exit status of this command because it *will*
    // fail with a "Must supply at least one output file" message.
    // Give it (only) a short time to run (everyonce in awhile it repeat logs
    // an error message, which runs deriver proc out of memory)
    // only keep the first 250 lines (in case it went crazy logging)
    $retval = null;

    $cmd = " $mplayer -identify -nosound -novideo -nolirc -frames 0 ".Util::esc($file);
    if ($iso)
      $cmd .= (Util::starts_with($file, 'br://') ? ' -bluray-device ' : ' -dvd-device ').Util::esc($iso);
    $cmd .= ' 2>/dev/null ';
    //error_log($cmd);
    $info = exec(Util::timeout(600)."$cmd | ".Util::timeout(600) . ' head -250', $lines, $retval);
    return implode($lines, "\n");
  }


  private static function pick_vstream($file, $out) {
    if (!preg_match_all('/\n *(Stream .*? Video: (.*?), (\d+)x(\d+)(, | \[)(.*))/', $out, $vlines))
      return '';

    //printr($vlines);
    if (count($vlines[1]) == 1)
      return $vlines[1][0];

    if (count($vlines[1]) < 1)
      return '';

    error_log("\nwarning: $file has 2+ video streams: " . print_r($vlines[1], 1));

    // videos can slide in motion JPEG sequences (eg: swf and m4a esp.)
    // if all but 1 of the video streams is motion JPEG, pick the outlier as the ideal video stream!
    // eg: /details/RobinSloanEPIC2014
    $idx = 0;
    $other = array_filter($vlines[1], function ($vline) use ($vlines, &$idx) {
      // reduce the full line down based on what the _codec_ parsed area of the full line shows
      return !Util::starts_with($vlines[2][$idx++], 'mjpeg,');
    });
    if (count($other) == 1) {
      $vline = $other[0];
      error_log("using sole non-mjpeg video stream:\n $vline\n");
      return $vline; // there is a single non-mjpeg video stream!  let's use it
    }


    // now use ffmpeg (which has greatly improved in its 'picking' since Ye Olden Days)
    // to pick which of the video streams is the best one (Tracey Feb2014)
    $cmd = (Util::timeout(600) . ' ' . Video::ffmpeg($file) . ' -i ' .
            Util::esc($file) . ' -an -f null -frames 0 /dev/null 2>&1 | ' .
            Util::timeout(600) . "fgrep -A10 'Stream mapping'");
    error_log($cmd);
    $tmp = Util::cmd($cmd, 'PRINT', 'CONTINUE');
    if (!preg_match('@\n\s*Stream #([\d:]+) \-> #0:0 @', $tmp, $mat))
      return '';

    $vstream = "Stream #{$mat[1]}";
    //error_log("Looking for $vstream");
    foreach ($vlines[1] as $tmp) {
      if (strncmp(Strings::killspace($tmp), $vstream, strlen($vstream)) === 0) {
        $vline = $tmp;
        error_log("Picking: $vline");
        return $vline;
      }
    }

    return '';
  }


  // returns false; else truthy hashmap output of Video::identify()
  public static function okMP4($file, $verbose = false, $max_bitrate = 0/*kb/s*/, $tv = false) {
    $identify = Video::identify($file, false, ($tv ? 10000 : 1000), $tv);

    if ($identify['vc'] != 'h264'  ||
        ($identify['ac'] != 'aac'  &&  $identify['audio'] !== 'false')  ||
        !Util::starts_with($identify['pix'], 'yuv420p')) {
      if ($verbose)
        error_log(print_r($identify, 1));
      return false;
    }

    if ($max_bitrate) {
      $bitrate = round($identify['bytes'] / max(1, $identify['length']) * 8 / 1024);
      if ($bitrate > $max_bitrate) {
        if ($verbose)
          error_log("$bitrate kb/s > $max_bitrate kb/s: " . print_r($identify, 1));
        return false;
      }
    }

    return $identify;
  }



  // given a duration (in seconds), return something like "HH:MM:SS", eg:
  //  01:12:27    (for 1 hr, 12 minutes, 27 seconds)
  public static function hms($seconds, $omit00hr = false, $tenthSecond = false, $nDecimals = 0, $sep = '.') {
    $hr  = floor($seconds / 3600);
    $min = floor(($seconds - ($hr * 3600)) / 60);
    $sec = $seconds - ($hr * 3600) - ($min * 60);
    if (!$nDecimals  &&  $tenthSecond)
      $nDecimals = 1;
    $secfmt = ($nDecimals ? '%04.' . $nDecimals . 'f' : '%02d');
    $ret = ($omit00hr  &&  $hr == 0 ?
            sprintf("%02d:$secfmt", $min, $sec) :
            sprintf("%02d:%02d:$secfmt", $hr, $min, $sec));

    if ($nDecimals  &&  $sep !== '.')
      $ret = str_replace('.', $sep, $ret);

    return $ret;
  }


  public static function hms2sec($hms, $fractional = false) {
    if ($fractional && preg_match('/^(\d+):(\d+):([\d+\.]+),(\d+)$/', $hms, $m)) //eg: "01:13:01,939"
      return $m[1] * 3600 + $m[2] * 60 + $m[3] + floatval(".{$m[4]}");
    if (preg_match('/^(\d+):(\d+):([\d+\.]+)$/', $hms, $m)) //eg: "01:13:01"
      return $m[1] * 3600 + $m[2] * 60 + $m[3];
    if (preg_match('/^(\d+):([\d+\.]+)$/', $hms, $m)) //eg: "13:01"
      return $m[1] * 60 + $m[2];
    if (preg_match('/^([\d+\.]+)$/', $hms, $m)) //eg: '01'
      return intval($m[1]);
    return false;
  }


  public static function hhmmss2sec($hhmmss) {
    if (!preg_match('/^(\d\d)(\d\d)(\d\d)$/', $hhmmss, $m))
      return false;

    return self::hms2sec("{$m[1]}:{$m[2]}:{$m[3]}");
  }


  private static function tofloat($rate) {
    if (intval($rate) == 0)
      return 0;
    if (preg_match('=^([\d\.]+)/([\d\.]+)$=', $rate, $m))
      return round($m[1] / $m[2], 2);
    return 0;
  }


  /**
   * Uses 'exiftool' to identify characteristics of file.
   *
   * @param string $file
   * @param boolean $quiet
   * @return mixed - hashmap from json output of tool; false on errors.
   */
  public static function EXIF($file, $quiet = false) {
    list($retval, $js) = Util::cmd(
      Util::timeout(7200) . 'exiftool -json ' . Util::esc($file) . ($quiet ? ' 2>/dev/null' : ''),
      'CONTINUE',
      'EXIT_STATUS'
    );
    if ($retval)
      return false;

    $ary = json_decode($js, true);
    if (count($ary))
      return $ary[0];

    return false;
  }


  /**
   * Tries to return fps of file, using 'exiftool'.
   *
   * @param string $file
   * @return mixed - string or false on errors
   */
  public static function fpsEXIF($file) {
    if (!($exif = Video::EXIF($file, true)))
      return false;

    if (isset($exif['VideoFrameRate']))
      return rtrim($exif['VideoFrameRate'], ' fps');

    if (isset($exif['FrameRate']))
      return rtrim($exif['FrameRate'], ' fps');

    return false;
  }


  // uses 'ffprobe' and returns string like "30000/1001" (for ~29.97)
  // OR sometimes a float, depending!
  //
  // [vstream arg] caller can set this to peg fps parsing to specific stream when src has 2+ video streams in it!
  public static function fps($src, $vline = '', $vstream = '') {
    // /details/MITOCW1800intros
    // has 2 videos which confused ffprobe mar2012, but this fps line
    // *is* present in the Video: line.  if any video is basically claiming
    // that in the identify line, we go with that!
    if (preg_match('/, 29.97 (fps|tbr),/', $vline))//eg: /details/Rage2012_201402
      return 29.97;
    if (preg_match('/, 30 (fps|tbr),/', $vline)) //eg: /details/canon-hf-10-test-2008_09_23a
      return 30;

    $fps = -666;
    if (preg_match('/, ([\d\.]+) fps,/', $vline, $mat))
      $fps = $mat[1]; // fallback -- we can usually be more accurate than this!!

    $ffprobe = Config::petaboxPath('bin-ffprobe').(Video::file_is_tv($src) ? ' -f mpegts ' : '');
    //error_log($ffprobe);

    $cmd = "$ffprobe -print_format json -show_streams " . Util::esc($src) . '  2>/dev/null';
    $str = Util::cmd($cmd, 'WARN');
    $streams = json_decode($str, 1);
    if (!$streams) {
      // UGH!!
      $str = mb_convert_encoding($str, 'UTF-8', 'ISO-8891-1,UTF-8');
      $streams = json_decode($str, 1);
    }
    if (!$streams)
      return ''; // eg: /details/fireworks_poster
    //printr($streams);

    $astreams = $vstreams = $goods = [];
    foreach ($streams['streams'] as $ary) {
      if ($ary['codec_type'] == 'video') {
        // If not really parseable stream, we'll effectively note that for later..
        if ($ary['width'] !== '0')
          $goods[] = $ary;

        $vstreams[] = $ary;
      }

      if ($ary['codec_type'] == 'audio')
        $astreams[] = $ary;
    }

    if (count($vstreams) > 1  &&  count($goods) > 0  &&
        count($vstreams) > count($goods)) {
      // throw out any video stream whose dimensions cant be parsed!
      // eg: /details/FOXNEWS_20091125_020000_Hannity
      $vstreams = $goods;
    }


    if ($vstream !== ''  &&  count($vstreams) > 1  &&  count($goods) == count($vstreams)) {
      foreach ($goods as $good) {
        if ($good['index'] === preg_replace('/^0\:/', '', $vstream)) {
          $vstreams = [$good];
          break;
        }
      }
    }



    $ptr = (count($vstreams) ? $vstreams : $astreams);
    //printr($ptr);



    // 'r' vs 'avg' discrepancy items, eg:
    //    /details/The_Phantom_Empire_1935
    if (count($ptr) == 1  &&
        isset($ptr[0]['r_frame_rate'])  &&
        isset($ptr[0]['avg_frame_rate'])) {
      $r   = Video::tofloat($ptr[0]['r_frame_rate']);
      $avg = Video::tofloat($ptr[0]['avg_frame_rate']);
      //error_log("r_frame_rate: $r vs avg_frame_rate: $avg ....");

      if ($r < 1  &&  $avg < 1  &&  ($ret = Video::fpsEXIF($src)))
        return $ret;

      if ($r <= 1    &&  $avg > 0  &&  $avg < 100)
        return $ptr[0]['avg_frame_rate'];

      if ($r > 100  &&  $avg > 0  &&  $avg < 100)
        return $ptr[0]['avg_frame_rate'];
    }



    $rates = [];
    foreach ($ptr as $stream) {
      foreach ($stream as $key => $val) {
        if (preg_match('/frame_rate$/', $key)) {
          $rate = $val;

          if (intval($rate) != 0) {
            // take the 1st one found (almost always 'r_frame_rate')

            // check for wack-ness
            // Eg: quite a few ".m4a" files which seem to be mostly m4a audio
            // but have a video motion-JPEG frame very very infrequently.
            // anyway, they aren't no 600 fps!!
            if ($rate == '600/1'  ||  // /details/Podcast7_492
                $rate == '600/2094059') { // /details/OvercomePt.3-OvercomeTheWorldWithLove_787
              $rate = '6/1';
            } elseif (($rate == '60000/1001'  ||  $rate == '359/6')  &&
                     $stream['codec_time_base'] == '1/2997') {
              $rate = '30000/1001'; // eg: /details/ExcerptFromASacredDuty
            }

            $rates[] = $rate;
          }
        }
      }
    }
    //error_log("Still trying... ".print_r($rates, 1));


    if (!count($rates))
      return '';

    // pick first rate
    $ret = $rates[0];
    $check = Video::tofloat($ret);
    return ($check > 100  &&  $fps > 0 && $fps < 100 ? $fps : $ret);
  }




  /**
   * Runs ffmpeg cmd to create frames for a video, suitable for thumbnails or an animated GIF.
   *
   * NOTE: throws exception on errors
   *
   * @param string $file - video input file
   * @param string $outDir - should always be passed in
   * @param boolean $tv - should always be passed in
   * @param int $maxThumbs - should always be passed in
   * @param array $identify - should always be passed in - @see Video::identify()
   * @param string $timestampBaseName
   * @param int $width
   * @param string $xtraArgs
   * @return int list of images made on SUCCESS, null if maker hangs and timesout
   */
  public static function frames(
    $file,
    $outDir = '.',
    $tv = false,
    $maxThumbs = self::MAX_THUMBS_PER_ITEM,
    $identify = null,
    $timestampBaseName = '',
    $width = 720,
    $xtraArgs = '-b:v 50K'
  ) {
    if ($tv  &&  TV::is_audio_only(basename($file), $identify))
      return null;

    // There can be MPEG-TS source frame size changes that caused the "-vf crop" filter to fail.  example:
    // /details/GALA_20121006_100000_Programa_Pagado
    //   Input stream #0:0 frame changed from size:720x480 fmt:yuv420p to size:528x480 fmt:yuv420pop=44410
    //   Input stream #0:0 frame changed from size:528x480 fmt:yuv420p to size:720x480 fmt:yuv420pop=46386
    // So if we can't do the *ideal* of black padding and TV cropping, give up
    // and just do a (nonideal) straight (possibly aspect distorting!) scale.
    // For non-TV, if these end up being the same command, we just uniq in 'tries' below...
    $vf = self::vf(['width' => $width, 'tv' => $tv, 'h264' => false, 'keyframes' => true]);

    $length = ($identify['length'] ? $identify['length'] : 0);


    // determine the 'rate' -- the number of frames/sec to extract.
    // max limit for number of thumbs.  sometimes the deriver misinterprets
    // certain video formats and cannot get the "every 30 seconds" correct
    // and does things like "every 30 frames" or whatnot.
    // max limit used to be 1000, but that's still a lot of frames when there
    // is this bug.
    // so a 2 hour film == 120 minutes == 240 frames (for every 30 seconds...)
    if ($tv) {
      $frameLimit = 5000;
      $frameEvery = 30;
    } else {
      $frameLimit = 250;

      $frameEvery = 30; // default is frame every 30 seconds
      if ($length == 0)
        $frameLimit = 30;
      elseif ($length < 5)
        $frameEvery = 1; // short clip -- frame every second
      elseif ($length < 120)
        $frameEvery = 5; // short film -- frame every 5 seconds
      elseif ($length > 900)
        $frameEvery = 60; // movie is over 15 minutes -- make a frame every minute
    }


    if ($length  &&  $maxThumbs > 0) {
      $frameEvery2 = ($length / $maxThumbs);
      if ($frameEvery2 > $frameEvery) {
        $frameEvery = $frameEvery2;
        echo "NOTE: dropping to 1 frame every $frameEvery seconds.\n";
      }
    }
    // NOTE: $frameEvery may be a float not an int!


    $rate = 1 / $frameEvery;
    echo("\n    Thumbnail maker: length=".Util::duration($length, 1).
         "  =>  Thumb every $frameEvery seconds (rate:$rate)\n\n");

    // cmd to make the thumbnails
    $frameLimit = "-vframes $frameLimit";
    if ($rate < 1) {
      // newer ffmpeg doesn't seem to be able to handle a rate < 1 w/o a semi
      // arbitrary '-b'(itrate) arg
      $rate .= " $xtraArgs ";
    }



    // '-pix_fmt' arg is to avoid any yuvj444p issues on output that ImageMagick
    // and gifsicle *both* cannot handle, eg:
    //   /details/prueba-02
    $cmd = Video::ffmpegV0($file) . ' -i ' . Util::esc($file) .
      (isset($identify['vstreams']) ? ' -map '.strtok($identify['vstreams'], ',') : '').
      " -an $vf $frameLimit -pix_fmt yuvj422p -r $rate $outDir/%08d.jpg";

    // normally we'd like to kill/timeout after 1 hour.
    // however aug2009, /details/scipy09_advancedTutorialDay1_4
    // that was too short.
    //
    // So, if we *know* the video is quite long, scale up 1.5x in hours
    //   1 hr ==> 1.5 hr timeout
    //   2 hr ==> 3   hr timeout
    //   3 hr ==> 4.5 hr timeout
    // (min 1 hours for timeout)
    $HD = ($tv  &&  $identify['height'] >= 720);
    $maxtime = Util::timeoutTime($length, ($HD ? '6x' : '1.5x'), ($HD ? '4h' : '2h'), $identify['bytes']);


    // Some items have so much damage they can fail deep into the
    // extraction and segfault ffmpeg when deinterlace-ing, eg:
    //    /details/TELECONGO_20110812_190000
    // so try failures one more time w/o the deinterlacing...
    $tries = [];
    $tries[] = $cmd;

    $tries[] = self::vf_nix($tries[count($tries) - 1], self::VF_YADIF);
    $tries[] = self::vf_nix($tries[count($tries) - 1], self::VF_PTS);
    $tries[] = str_replace(' -i ', ' -err_detect 0 -i ',          $tries[count($tries) - 1]);
    // Next try is severe, but some items have *SO* much packet damage, we'd rather get
    // any images we can at all, eg: /details/WPVI_20140803_090000_Puerto_Rican_Panorama
    $tries[] = str_replace(' -i ', ' -fflags discardcorrupt -i ', $tries[count($tries) - 1]);
    // Rare videos choke in I-frames only filter - eg: /details/prueba-02
    $tries[] = self::vf_nix($tries[count($tries) - 1], self::VF_IFRAMES);

    $tries = array_values(array_unique($tries)); // nix dupes; retains order!
    //printr($tries);

    foreach ($tries as $idx => $cmd) {
      // TV archive has special behaviour if last attempt fails...
      $last = ($idx == (count($tries) - 1));
      if (($err = Util::cmdQT($cmd, $maxtime, 'CONTINUE'))) {
        if ($last  &&  $tv) {
          $made = glob("$outDir/*.jpg");
          if (count($made) < 2)
            throw new Exception('failed to make thumbnails');
          printr($made);
          echo '  FAILED but made ' . count($made) . " thumbs, so moving on with them...\n";
          $err = false;
          break;
        }
        echo "   ==> FAILED!\n";
      } elseif (!count(glob("$outDir/*.jpg"))) {
        echo "   ==> NO THUMBS MADE!\n";
      } else {
        if ($last  &&  $tv) {
          // track the number of shows per center and channel that have heavy packet loss!
          $id = pathinfo($file, PATHINFO_FILENAME);
          $TVC = TV::id2tvc($id);
          list(, $chan) = TV::identifierTS2($id);
          StatsD::increment("badTV.$TVC");
          StatsD::increment("badTV.$chan");
        }
        break;
      }
    }
    if ($err)
      return null; // ffmpeg failed or, more likely, timed out!


    $thumbnails = glob("$outDir/*.jpg");
    sort($thumbnails);

    if (!count($thumbnails))
      throw new Exception('No thumbnails made');


    // Mar2007 -- switch from mplayer to ffmpeg
    // an improvement!  however, strangely, the first interval of the film
    // always seems to have 2 thumbs, not 1 (as it should) and they are often
    // "nearly the same".  so let's remove thumbnail frame ..0001
    if (count($thumbnails) > 1) {
      `rm -f {$thumbnails[0]}`;
      unset($thumbnails[0]);
    }


    $timestamps = false;
    if ($timestampBaseName  &&  $length  &&  $frameEvery > 1.5) {
      $expected = ($length / $frameEvery);
      if (($tv  &&  $expected > 5)  ||
          ($expected > 1  &&  abs(count($thumbnails) - $expected) <= 3)) {
        $timestamps = true;
        echo "\n    Will use timestamps for created thumb filenames\n";
      } else {
        echo "\n    Not using timestamps -- expected:$expected; made:".
          count($thumbnails)."\n";
      }
    }

    if (!$length  &&  $maxThumbs > 0  &&  count($thumbnails) > $maxThumbs) {
      echo 'NOTE: Thinning out the ' . count($thumbnails) . " thumbnails to $maxThumbs\n";
      $thumbnails = Util::sampleList($thumbnails, $maxThumbs);
    }

    if ($maxThumbs > 0  &&  count($thumbnails) > $maxThumbs) {
      // for a movie with no determined length (so we used "every 30 seconds")
      // we may have made too many frames.
      // take the first $maxThumbs slices (and throw out the rest)
      echo "NOTE: Made too many!  skipping 1st, then taking next $maxThumbs (of ".
        count($thumbnails).") frames.\n";
      $thumbnails = array_slice($thumbnails, 1, $maxThumbs + 1);
    }


    if ($timestampBaseName) {
      $first = true;
      $i = 0;
      $ary = $thumbnails;
      $thumbnails = [];
      foreach ($ary as $thumbnail) {
        // if we are using timestamps, use timecode (seconds) from start for name
        // else make 1st thumb ..001; 2nd ..002; etc...
        if ($timestamps) {
          if ($i == 0) {
            $sec = 1;
          } else {
            $sec = intval(basename($thumbnail));//newer version, named based on seconds
            if ($sec < 0)
              $sec = round($i * $frameEvery);
          }
        }

        $num = ($timestamps ? $sec : $i + 1);

        echo basename($thumbnail)." ==> $num\n";

        $thumbFile = $timestampBaseName . sprintf('%06d', $num) . '.jpg';
        Util::cmd("mv $thumbnail ".Util::esc($thumbFile), 'THROW');
        // NOTE: subtle!  2+ frames *could* end up being later considered
        // the same filename, eg: /details/ShaneBrennanSittingTourBreathe
        // so we'll pick the *last* one when 2+ frames converge on same name
        $thumbnails[$num] = $thumbFile;
        $i++;
      }
    }


    return array_values($thumbnails);
  }



  /**
   * Removed a VF_* video filter from an ffmpeg command string
   *
   * @param string $cmd
   * @param string $vf - one of the constants like: VF_YADIF
   * @return string
   */
  public static function vf_nix($cmd, $vf) {
    $vf = (array)$vf;
    foreach ($vf as $str)
      $cmd = str_replace("$str,", '', $cmd);
    return $cmd;
  }

  // video filters
  const VF_PTS     = 'setpts=PTS-STARTPTS';
  const VF_YADIF   = 'yadif'; // yadif=0:-1:1  used at one point..
  const VF_SQUARE  = "'scale=trunc(iw*sar/2)*2:trunc(ih/2)*2'";
  const VF_SAR     = 'setsar=1/1';
  const VF_TOP4    = 'crop=iw:ih-4:0:4';
  const VF_IFRAMES = "'select=eq(pict_type\,I)'";

  /**
   * Returns string with list of ffmpeg -vf video filters to use, taking care of
   * deinterlacing, source rectangular pixels, even width/height, and more.
   *
   * Some source files have non-4:3 aspect ratio (w/ 'square'/normal pixels).
   *
   * Some source files have non-4:3 aspect ratio (w/ 'rectangular' pixels) like
   * DV video or anamorphic mpeg2 dvds.
   * Some of those files have 4:3 or 16:9 aspect ratio BUT have "rectangular pixels".
   *
   * @param integer $width      pass in either width or height (eg: use height=480 for 480p)
   * @param integer $height     pass in either width or height (eg: use height=480 for 480p)
   * @param boolean $tv         is file a TV recording?
   * @param boolean $h264       are we creating mp4?
   * @param boolean $keyframes  drop non-keyframes? (eg: thumbnailing)
   * @return void
   */
  public static function vf(
    $width = -2,
    $height = -2,
    $tv = false,
    $h264 = false,
    $keyframes = false
  ) {
    extract(Arg::destructure(), EXTR_REFS);
    $filters = [];

    // hmm, this *helps* for Thumbnails and tricky MPEG-TS items, but
    // not reliable enough for mp4 for some shows, eg:
    //   /details/MSNBCW_20120502_010000_The_Rachel_Maddow_Show
    if ($tv  &&  !$h264) $filters[] = self::VF_PTS;


    // OK, it's prolly more ideal for majority of videos to do this:
    //   $filters = ['yadif=0:-1:1'];
    // and *not* deinterlace if the source is progressive (or claims to be).
    // However, for videos that have effectively "burned in" a bad interlace
    // into a progressive frame (eg: got the top/bottom field first wrong)
    // the output is nearly unwatchable.  So deinterlace alwayz...
    //   Eg:  /details/dom-25917-watchthisshow-digitalspin1996
    $filters[] = self::VF_YADIF;


    if ($keyframes)
      $filters[] = self::VF_IFRAMES; // xxx iframes only -- but _wanted_ gap/spacing..

    // convert any rectangular pixels to square - and make sure WxH are not odd numbers
    $filters[] = self::VF_SQUARE;
    $filters[] = self::VF_SAR;

    if ($tv) {
      // for TV archive videos -- we want to remove the top 4 pixels of the video
      // which frequently contain programming information 'hidden' in them, making
      // them look like static at the top of the image
      $filters[] = self::VF_TOP4;
    }

    $filters[] = ($width > 0 ?
      "'scale=min($width\,iw):-2'" :
      "'scale=-2:min($height\,ih)'"
    );
    // printr($filters);

    return '-vf ' . join(',', $filters); // . ($keyframes ? ' -vsync 0'/*xxx*/ : '');
  }


  public static function ffmpeg_params(
    $cmd,
    $identify,
    $sourceFormat,
    $acoder,
    $params = '640x480',
    $tvarchive = false,
    $h264 = false
  ) {
    if (!$identify['width']  ||  !$identify['height']) {
      if ($tvarchive)
        fatal(TV::$ERRS[2]);

      fatal('Video width/height not calculable -- likely no video track!');
    }

    if (!preg_match('/^(\d)+x(\d+)(.*)/', $params, $mat))
      fatal("bad params! [$params]");
    list(, $width, $height, $rest) = $mat;


    $vf = self::vf(['height' => $height, 'tv' => $tvarchive, 'h264' => $h264]);


    $rate = ($identify['fps'] ? " -r {$identify['fps']} " : '');
    //$rate = '';//xxxx


    // Traction control doesn't allow you to drift your RWD car, get it?  8-)
    $traction_control = ($identify['audio'] !== 'false'  &&
                         ($tvarchive  ||  (isset($identify['vsync'])  &&  $identify['vsync'] == 1)  ||
                          in_array($sourceFormat, ['MPEG2','MPEG1','h.264/MPEG2-TS','MPEG2-TS']))
                         ? Video::AV_ANTI_DRIFT : false);

    $args = (" $rest " . ($traction_control ? $traction_control : $rate) .
             (isset($identify['DAR']) ? " -aspect {$identify['DAR']}" : '').
             ' -map_metadata -1,g:0,g');


    // normally, we'd be ready to run!  however, quick test first second...
    $tmpf = '/tmp/rawvidtmp.' . getmypid() . '.avi';
    $retval = Util::cmdQT("$cmd $vf $args -an -t 1.0 -y $tmpf", 0.5, 'WARN');
    `rm -f $tmpf`;
    if ($retval) {
      // OK this can *also* fail as of (oneiric) Nov2011 when also using
      // 'baseline' profile (which we do!), if source is yuv 422, eg:
      //    /details/AlcTestClip
      // or source is yuv 444, eg:
      //    /details/SamsFrames
      $args .= ' -pix_fmt yuv420p ';
    }


    // now figure out the audio params
    if ($identify['audio']  &&  $identify['audio'] != 'false') {
      // for archive items (esp. TV), always use 1st audio track IFF 2+
      if (isset($identify['astreams'])  &&  isset($identify['vstreams'])) {
        $vid = str_replace('.', ':', strtok($identify['vstreams'], ','));
        $aid = str_replace('.', ':', strtok($identify['astreams'], ','));

        $map = ' -map '.$vid.($traction_control ? ",$vid" : '');

        // We've repeatedly had rare DV original files will questionable
        // audio layout -- where it is 2 or 4 mono channels, appearing in
        // the A/V containter as the same number of audio streams, where
        // sometimes the 1st mono stream is 'blank', which gets picked, and
        // thus derivatives have no audio output!
        // If it appears it is this case, 'downmix' all input audio channels
        // to single mono channel output and be done with this little box of horror!
        //   eg: /details/XFR_2013-07-26_2A_01
        $monosDV = ((@$identify['vc'] == 'dvvideo'  ||
                     @$identify['vc'] == 'v210')  &&  // eg: /details/XFR_2013-09-07_1B_01
                    ((@$identify['audio_nchannels'] == 2  &&  count(@explode(',', @$identify['astreams'])) == 2)  ||
                     (@$identify['audio_nchannels'] == 4  &&  count(@explode(',', @$identify['astreams'])) == 4)));
        $ac = '';
        if ($monosDV) {
          echo "\nWARNING! DV MONO 2/4 CHANNELS DETECTED!  DOWNMIX ALL TO MONO!\n\n";
          $ac = '-ac 1 '; // bleah write arg *twice* because $acoder typically has `-ac 2` in it now
          $map .= ' -filter_complex "[0]amerge[aout]" -map "[aout]" ' . $ac;
        } else {
          // The 2nd specifier (after comma) is for 'sync_file_id'
          // and *both* should indeed be using the *vid*
          $map .= ' -map '.$aid.($traction_control ? ",$vid" : '');
        }

        $acoder = "$map $acoder $ac";
      }
    } else {
      // no audio!
      $acoder = '-an';
      if ($tvarchive) {
        // rare, but /details/KICU_20110322_110000_Paid_Programming ...
        echo "\nWARNING! NO CHANNELS FOUND IN AUDIO STREAM(S)!\n\n";
      }
    }


    // This item failed pass 1 but worked when removed the more experimental
    // efforts to avoid A/V drift...
    //    /details/KICU_20110602_153000_Heidi_Klums_Skin_Secrets
    // NOTE: /details/KSTS_20110606_020000_Las_Cronicas_de_Spiderwick failed
    // pass 1 *sporadically* but passed 1x; also passed w/ '-deinterlace' nixed
    // (we shifted to yadif so possibly worth a shot to retry w/o...)
    $tries = [];
    $tries[] = "$cmd $vf $args";

    // Avoid (rare) .mp4 deriving fails w/ items w/ damage/nonideal in (esp.) their starting minute,
    //    [libfdk_aac @ 0x8a87780] 2 frames left in the queue on closing
    // eg: /details/KPIX_20200822_060000_KPIX_5_News_at_11pm
    if ($tvarchive)
      $tries[] = "$cmd $vf -max_muxing_queue_size 512 $args";

    // For MPEG-TS, sometimes the pts 'clock' can 'rollover' and cause us to fail.
    // So to workaround, reset pts to start at '0' and ever increase.
    // Eg: head -20000000 /details/../BBC2_20121219_232000_Stuffed__The_Great_British_Christmas....mpg
    // nicely shows the "clock rollover" (and this worked around failed .mp4 creation!)
    if (!strpos($tries[count($tries) - 1], self::VF_PTS))
      $tries[] = str_replace(' -vf ', ' -vf '.self::VF_PTS.',', $tries[count($tries) - 1]);
    if ($traction_control)
      $tries[] = $tries[count($tries) - 1] . $rate;
    $tries[] = self::vf_nix($tries[count($tries) - 1], self::VF_YADIF);
    $tries[] = str_replace(' -i ', ' -err_detect 0 -i ', $tries[count($tries) - 1]);

    // /details/ALMAGHRIBIA_20190116_200000
    $tries[] = self::vf_nix($tries[count($tries) - 1], [self::VF_SQUARE, self::VF_SAR, self::VF_TOP4]);

    $tries[] = str_replace(self::AV_ANTI_DRIFT, ' ', $tries[count($tries) - 1]);

    $tries = array_values(array_unique($tries)); // nix dupes; retains order!
    //printr($tries);

    return [$tries, $acoder];
  }


  // This is used (only) by the deriver by video modules.
  // very similar to "metadataString()", but with an array of mapped names...
  // see also:   http://wiki.multimedia.cx/index.php?title=FFmpeg_Metadata
  private static $videoMetaCache = null;
  public static function videoMeta($module, $mapping, $addurl = false) {
    $url = "https://archive.org/details/{$module->identifier}";

    // add any relevant metadata
    if (!self::$videoMetaCache) {
      // parse the _meta.xml
      self::$videoMetaCache = [];
      $xml = simplexml_load_file("{$module->itemDir}{$module->identifier}_meta.xml");
      if ($xml) {
        foreach ($xml->children() as $child) {
          $tag = $child->getName();
          $val = trim((string)$child[0]);
          if ($val  &&  !isset(self::$videoMetaCache[$tag]))//keep 1st instance when 2 elems
            self::$videoMetaCache[$tag] = [$val];
        }
      }

      // if no title, make it the /details/ url
      if (!isset(self::$videoMetaCache['title']))
        self::$videoMetaCache['title'] = [$url];
    }


    $m = '';
    foreach ($mapping as $metaName => $pre) {
      if ($metaName == 'date'  &&  $pre == '-metadata year=') {
        // special case extra needed check to make sure it's a 4-digit number!
        if (!self::$videoMetaCache['date']) continue;
        if (!preg_match('/^\d\d\d\d$/', self::$videoMetaCache['date'][0])) continue;
      }

      // append /details/ url to the title, if desired (and not already the title)
      $valpost = (($addurl  &&  $metaName == 'title'  &&
                   self::$videoMetaCache['title'][0] != $url) ? " - $url" : '');

      // try to get any **<file> tag specific** <title> element first.  if none,
      // get the overall item's <title>
      if ($tmp = self::metadataString($module->metadata, $metaName, $pre, '', $valpost)) {
        $m .= " $tmp";
      } else {
        $m .= ' ' . self::metadataString(self::$videoMetaCache, $metaName, $pre, '', $valpost);
      }
    }

    return $m;
  }


  // Only used by video and audio deriver modules.
  // Use a tag/key from the metadata (IFF it exists)
  // and return a string like [returned w/o quotes]:
  //    "$pre[TAG VALUE]$post"
  // suitable for a cmd-line invocation.
  // If tag is not set in metadata, return ''
  public static function metadataString($metadata, $tag, $pre = '', $post = '', $valpost = '') {
    if ($metadata  &&  isset($metadata[$tag])) {
      if (is_array($metadata[$tag])) { /*SHOULD always be true!*/
        $val = $metadata[$tag][0];
      } else {
        $val = $metadata[$tag];
      }

      // NOTE: we use newer/better way to escape args...
      return $pre.Util::esc($val.$valpost).$post;
    }
    return '';
  }


  public static function regress($all = false) {
    self::assert_setup();

    chdir('/tmp') || fatal('no /tmp?');

    assert(Video::hms(13.3) == '00:00:13');
    assert(Video::hms(13.3, 1) == '00:13');
    assert(Video::hms(13.3, 1, 1) == '00:13.3');
    assert(Video::hms(13.3, 0, 1) == '00:00:13.3');
    assert(Video::hms(13.9, 0, 1) == '00:00:13.9');
    assert(Video::hms(13.9, 0, 0, 3) == '00:00:13.900');
    assert(Video::hms(13.9, 0, 1, 3) == '00:00:13.900');
    assert(Video::hms(13.9, 0, 1, 3, '.') == '00:00:13.900');
    assert(Video::hms(13.9, 0, 0, 3, ',') == '00:00:13,900');

    assert(Video::hhmmss2sec('021304') == 7984);


    if (!$all)
      return;


    // rectangular pixel, aspect ratio, scale and cropping tests
    $ffmpeg  = Config::petaboxPath('bin-ffmpeg');
    $ffprobe = Config::petaboxPath('bin-ffprobe');
    foreach ([
      // W x H    SAR/PAR   DAR    VF CONF         W x H
      ['320x240',   '1:1',  '4:3', 'h480',  /*=>*/ '320x240'], // eg: small simple video
      ['320x240',   '1:1',  '4:3', 'h480tv',/*=>*/ '320x236'], // eg: small simple video
      ['320x240',   '1:1',  '4:3', 'w720',  /*=>*/ '320x240'], // eg: small simple video
      ['320x240',   '1:1',  '4:3', 'w720tv',/*=>*/ '320x236'], // eg: small simple video

      ['704x576', '16:11', '16:9', 'h480',  /*=>*/ '854x480'], // eg: BBCNEWS   16:9 at 576p
      ['704x576', '16:11', '16:9', 'h480tv',/*=>*/ '860x480'], // eg: BBCNEWS   16:9 at 576p
      ['704x576', '16:11', '16:9', 'w720',  /*=>*/ '720x406'], // eg: BBCNEWS   16:9 at 576p
      ['704x576', '16:11', '16:9', 'w720tv',/*=>*/ '720x402'], // eg: BBCNEWS   16:9 at 576p

      ['1920x1080', '1:1', '16:9', 'h480',  /*=>*/ '854x480'], // eg: KNTV (SF) 16:9 at 1080p
      ['1920x1080', '1:1', '16:9', 'h480tv',/*=>*/ '856x480'], // eg: KNTV (SF) 16:9 at 1080p
      ['1920x1080', '1:1', '16:9', 'w720',  /*=>*/ '720x406'], // eg: KNTV (SF) 16:9 at 1080p
      ['1920x1080', '1:1', '16:9', 'w720tv',/*=>*/ '720x404'], // eg: KNTV (SF) 16:9 at 1080p

      ['720x480',   '8:9',  '4:3', 'h480',  /*=>*/ '640x480'], // eg: BLOOMBERG (SF) at 480p
      ['720x480',   '8:9',  '4:3', 'h480tv',/*=>*/ '640x476'], // eg: BLOOMBERG (SF) at 480p
      ['720x480',   '8:9',  '4:3', 'w720',  /*=>*/ '640x480'], // eg: BLOOMBERG (SF) at 480p
      ['720x480',   '8:9',  '4:3', 'w720tv',/*=>*/ '640x476'], // eg: BLOOMBERG (SF) at 480p

      ['704x480',  '10:11', '4:3', 'h480tv',/*=>*/ '640x476'], // eg: KNTV (RMD) at 480p

    ] as $ary) {
      list($wh,      $sar,   $dar, $conf, $outWH) = $ary;

      $args = [
        'tv'     => (strpos($conf, 'tv') ? 1 : 0),
        'width'  => ($conf[0] === 'w' ? preg_replace('/[^\d]/', '', $conf) : -2),
        'height' => ($conf[0] === 'h' ? preg_replace('/[^\d]/', '', $conf) : -2),
      ];
      printr($ary);
      printr($args);
      $vf = self::vf($args);


      if (0) {
        // create .1s test pattern video
        Util::cmdPP(
          "$ffmpeg -y -v 0 -f lavfi -i testsrc=d=0.1:s=$wh:r=24,format=yuv420p -vf setdar=$dar,setsar=$sar in.mp4"
        );
      }

      Util::cmdPP("convert -size $wh xc:black frame.png");
      Util::cmdPP("$ffmpeg -y -v 0 -r 1 -i frame.png -vf setsar=$sar,setdar=$dar in.mp4");
      Util::cmdPP("$ffmpeg -y -v 0 -i in.mp4 $vf out.mp4");


      $identify = Video::identify('out.mp4');
      foreach ($identify as $k => $v) {
        if (!in_array($k, ['width', 'height', 'DAR', 'PAR']))
          unset($identify[$k]);
      }
      printr($identify);

      // non TV should _generally_ be exactly 1:1 -- and TV should be nearly 1:1
      $par = $identify['PAR'];
      list($x, $y) = explode(':', $par);
      assert(round($x / $y) === 1.0);

      // DAR should not change
      if ($args['tv']) {
        // TV should be _really_ close, if not exact
        list($x, $y) = explode(':', $dar);
        $i = $x / $y;
        list($x, $y) = explode(':', $identify['DAR']);
        $o = $x / $y;
        assert(abs($i - $o) < .05);
      } else {
        assert($identify['DAR'] === $dar);
      }

      $out = "{$identify['width']}x{$identify['height']}";
      error_log("$outWH (expected) -v- $out (output)");
      assert($out === $outWH);
    }

    if ($all === 'vf')
      return;


    // do some *very* basic testing of ffprobe (of all the current ways as of July2014 we use it)
    // @codingStandardsIgnoreLine
    $mpg = '/home/tracey/public_html/_/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric.mpg';
    foreach (["-print_format compact -select_streams v -show_packets | egrep -o '.*flags=.' | head -20" =>
              // @codingStandardsIgnoreStart
              'packet|codec_type=video|stream_index=0|pts=8286398944|pts_time=92071.099378|dts=8286389935|dts_time=92070.999278|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=45044|pos=2820|flags=K
packet|codec_type=video|stream_index=0|pts=8286392938|pts_time=92071.032644|dts=8286392938|dts_time=92071.032644|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=1415|pos=52452|flags=_
packet|codec_type=video|stream_index=0|pts=8286395941|pts_time=92071.066011|dts=8286395941|dts_time=92071.066011|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=725|pos=53956|flags=_
packet|codec_type=video|stream_index=0|pts=8286407953|pts_time=92071.199478|dts=8286398944|dts_time=92071.099378|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=7303|pos=55084|flags=_
packet|codec_type=video|stream_index=0|pts=8286401947|pts_time=92071.132744|dts=8286401947|dts_time=92071.132744|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=967|pos=62604|flags=_
packet|codec_type=video|stream_index=0|pts=8286404950|pts_time=92071.166111|dts=8286404950|dts_time=92071.166111|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=1614|pos=63920|flags=_
packet|codec_type=video|stream_index=0|pts=8286416962|pts_time=92071.299578|dts=8286407953|dts_time=92071.199478|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=9822|pos=66928|flags=_
packet|codec_type=video|stream_index=0|pts=8286410956|pts_time=92071.232844|dts=8286410956|dts_time=92071.232844|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=1172|pos=77644|flags=_
packet|codec_type=video|stream_index=0|pts=8286413959|pts_time=92071.266211|dts=8286413959|dts_time=92071.266211|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=2725|pos=80464|flags=_
packet|codec_type=video|stream_index=0|pts=8286425971|pts_time=92071.399678|dts=8286416962|dts_time=92071.299578|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=17365|pos=83472|flags=_
packet|codec_type=video|stream_index=0|pts=8286419965|pts_time=92071.332944|dts=8286419965|dts_time=92071.332944|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=4783|pos=102084|flags=_
packet|codec_type=video|stream_index=0|pts=8286422968|pts_time=92071.366311|dts=8286422968|dts_time=92071.366311|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=4405|pos=107912|flags=_
packet|codec_type=video|stream_index=0|pts=8286434980|pts_time=92071.499778|dts=8286425971|dts_time=92071.399678|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=20582|pos=114116|flags=_
packet|codec_type=video|stream_index=0|pts=8286428974|pts_time=92071.433044|dts=8286428974|dts_time=92071.433044|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=5831|pos=135548|flags=_
packet|codec_type=video|stream_index=0|pts=8286431977|pts_time=92071.466411|dts=8286431977|dts_time=92071.466411|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=7829|pos=143632|flags=_
packet|codec_type=video|stream_index=0|pts=8286443989|pts_time=92071.599878|dts=8286434980|dts_time=92071.499778|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=57108|pos=151904|flags=K
packet|codec_type=video|stream_index=0|pts=8286437983|pts_time=92071.533144|dts=8286437983|dts_time=92071.533144|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=7318|pos=211688|flags=_
packet|codec_type=video|stream_index=0|pts=8286440986|pts_time=92071.566511|dts=8286440986|dts_time=92071.566511|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=7141|pos=219396|flags=_
packet|codec_type=video|stream_index=0|pts=8286452998|pts_time=92071.699978|dts=8286443989|dts_time=92071.599878|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=22151|pos=228044|flags=_
packet|codec_type=video|stream_index=0|pts=8286446992|pts_time=92071.633244|dts=8286446992|dts_time=92071.633244|duration=3003|duration_time=0.033367|convergence_duration=N/A|convergence_duration_time=N/A|size=7214|pos=254928|flags=_',


              '-print_format json -show_streams -select_streams v' =>
              '{"streams":[{"index":0,"codec_name":"mpeg2video","codec_long_name":"MPEG-2 video","profile":"Main","codec_type":"video","codec_time_base":"1001/30000","codec_tag_string":"[2][0][0][0]","codec_tag":"0x0002","width":528,"height":480,"closed_captions":1,"has_b_frames":1,"sample_aspect_ratio":"40:33","display_aspect_ratio":"4:3","pix_fmt":"yuv420p","level":8,"color_range":"tv","field_order":"progressive","id":"0x880","r_frame_rate":"30000/1001","avg_frame_rate":"30000/1001","time_base":"1/90000","start_pts":8286398944,"start_time":"92071.099378","duration_ts":165165,"duration":"1.835167","disposition":{"default":0,"dub":0,"original":0,"comment":0,"lyrics":0,"karaoke":0,"forced":0,"hearing_impaired":0,"visual_impaired":0,"clean_effects":0,"attached_pic":0,"timed_thumbnails":0},"side_data_list":[{"side_data_type":"CPB properties"}]}]}',
              // @codingStandardsIgnoreEnd


              '-print_format compact -show_format -show_entries stream=codec_type,id | egrep .' =>
                'program|stream|codec_type=video|id=0x880side_data|
stream|codec_type=audio|id=0x881
stream|codec_type=video|id=0x880side_data|
stream|codec_type=audio|id=0x881
format|filename=/home/tracey/public_html/_/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric.mpg|nb_streams=2|nb_programs=1|format_name=mpegts|format_long_name=MPEG-TS (MPEG-2 Transport Stream)|start_time=92070.357978|duration=2.576567|size=690148|bit_rate=2142845|probe_score=50',


              "-print_format compact -show_frames -select_streams v -show_entries frame=pict_type,pkt_pos | egrep -o '.*pict_type=.'" =>
'frame|pkt_pos=2820|pict_type=I
frame|pkt_pos=62604|pict_type=B
frame|pkt_pos=63920|pict_type=B
frame|pkt_pos=55084|pict_type=P
frame|pkt_pos=77644|pict_type=B
frame|pkt_pos=80464|pict_type=B
frame|pkt_pos=66928|pict_type=P
frame|pkt_pos=102084|pict_type=B
frame|pkt_pos=107912|pict_type=B
frame|pkt_pos=83472|pict_type=P
frame|pkt_pos=135548|pict_type=B
frame|pkt_pos=143632|pict_type=B
frame|pkt_pos=114116|pict_type=P
frame|pkt_pos=211688|pict_type=B
frame|pkt_pos=219396|pict_type=B
frame|pkt_pos=151904|pict_type=I
frame|pkt_pos=254928|pict_type=B
frame|pkt_pos=262636|pict_type=B
frame|pkt_pos=228044|pict_type=P
frame|pkt_pos=288016|pict_type=B
frame|pkt_pos=293844|pict_type=B
frame|pkt_pos=269592|pict_type=P
frame|pkt_pos=319412|pict_type=B
frame|pkt_pos=323924|pict_type=B
frame|pkt_pos=299860|pict_type=P
frame|pkt_pos=343476|pict_type=B
frame|pkt_pos=347612|pict_type=B
frame|pkt_pos=328248|pict_type=P
frame|pkt_pos=414540|pict_type=B
frame|pkt_pos=417548|pict_type=B
frame|pkt_pos=350996|pict_type=I
frame|pkt_pos=432024|pict_type=B
frame|pkt_pos=434656|pict_type=B
frame|pkt_pos=420368|pict_type=P
frame|pkt_pos=449508|pict_type=B
frame|pkt_pos=451012|pict_type=B
frame|pkt_pos=436912|pict_type=P
frame|pkt_pos=465300|pict_type=B
frame|pkt_pos=468120|pict_type=B
frame|pkt_pos=452516|pict_type=P
frame|pkt_pos=481280|pict_type=B
frame|pkt_pos=484100|pict_type=B
frame|pkt_pos=470940|pict_type=P
frame|pkt_pos=537116|pict_type=B
frame|pkt_pos=552344|pict_type=B
frame|pkt_pos=485604|pict_type=I
frame|pkt_pos=585808|pict_type=B
frame|pkt_pos=597464|pict_type=B
frame|pkt_pos=562872|pict_type=P
frame|pkt_pos=640516|pict_type=B
frame|pkt_pos=648788|pict_type=B
frame|pkt_pos=612316|pict_type=P
frame|pkt_pos=658752|pict_type=P',


              '-print_format json -show_streams' =>
              '{"streams":[{"index":0,"codec_name":"mpeg2video","codec_long_name":"MPEG-2 video","profile":"Main","codec_type":"video","codec_time_base":"1001/30000","codec_tag_string":"[2][0][0][0]","codec_tag":"0x0002","width":528,"height":480,"closed_captions":1,"has_b_frames":1,"sample_aspect_ratio":"40:33","display_aspect_ratio":"4:3","pix_fmt":"yuv420p","level":8,"color_range":"tv","field_order":"progressive","id":"0x880","r_frame_rate":"30000/1001","avg_frame_rate":"30000/1001","time_base":"1/90000","start_pts":8286398944,"start_time":"92071.099378","duration_ts":165165,"duration":"1.835167","disposition":{"default":0,"dub":0,"original":0,"comment":0,"lyrics":0,"karaoke":0,"forced":0,"hearing_impaired":0,"visual_impaired":0,"clean_effects":0,"attached_pic":0,"timed_thumbnails":0},"side_data_list":[{"side_data_type":"CPB properties"}]},{"index":1,"codec_name":"ac3","codec_long_name":"ATSC A/52A (AC-3)","codec_type":"audio","codec_time_base":"1/48000","codec_tag_string":"[129][0][0][0]","codec_tag":"0x0081","sample_fmt":"fltp","sample_rate":"48000","channels":2,"channel_layout":"stereo","bits_per_sample":0,"dmix_mode":"-1","ltrt_cmixlev":"-1.000000","ltrt_surmixlev":"-1.000000","loro_cmixlev":"-1.000000","loro_surmixlev":"-1.000000","id":"0x881","r_frame_rate":"0/0","avg_frame_rate":"0/0","time_base":"1/90000","start_pts":8286332218,"start_time":"92070.357978","duration_ts":161280,"duration":"1.792000","bit_rate":"192000","disposition":{"default":0,"dub":0,"original":0,"comment":0,"lyrics":0,"karaoke":0,"forced":0,"hearing_impaired":0,"visual_impaired":0,"clean_effects":0,"attached_pic":0,"timed_thumbnails":0}}]}',
    ] as $argsfull => $vals) {
      $pass = false;
      foreach ((array)$vals as $val) {
        list($args, $pipe1, $pipe2) = explode('|', $argsfull);
        $out = Util::cmd(
          "$ffprobe $args $mpg 2>/dev/null" .
          ($pipe1 ? " | $pipe1" : '') .
          ($pipe2 ? " | $pipe2" : ''),
          'PRINT'
        );
        $orig = $out;
        if ($out == $val) {
          $pass = true;
          break;
        }

        if (substr(ltrim($val), 0, 1) == '{') {
          $val = json_decode($val, 1);
          $out = json_decode($out, 1);

          // changed when went to wily!
          unset($out['streams'][0]['coded_width']);
          unset($out['streams'][0]['coded_height']);
          unset($out['streams'][0]['chroma_location']);
          unset($out['streams'][0]['refs']);


          if ($out == $val) {
            error_log('JSON match!');
            $pass = true;
            break;
          }
        }
      }

      if (!$pass) {
        file_put_contents('/tmp/video-regress-shouldBe', print_r($val, 1));
        file_put_contents('/tmp/video-regress-actually', print_r($out, 1));
        file_put_contents(
          '/tmp/video-regress-actually.json',
          json_encode($out, JSON_UNESCAPED_SLASHES) . "\n"
        );
        $cmd = 'colordiff -U30 /tmp/video-regress-shouldBe  /tmp/video-regress-actually';
        error_log('NOT EQUAL!');
        echo $orig;
        error_log($cmd);
        passthru($cmd);
        assert(false);
      }
    }


    error_log("\n\n\nSUCCESS!\n\n\n");
  }
}
