<?

class Video {
  use Assert;

  public static $PLAYABLES = null;


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
    $args = (defined('LIMITLESS') ? ' -threads 16 ' : ' -threads 2 ').(Video::file_is_tv($file) ? ' -f mpegts ':'');

    return "$ffmpeg $args1 -analyzeduration 900000000000 -probesize 200M $args";
  }


  // given a short or fully qualified filename (no CGI args!),
  // tries to determine if it *looks like* it's TV, pattern-wise
  // and returns true/false
  public static function file_is_tv($file = null) {
    return ($file!==null  &&  preg_match('=([^/]+)\.mpg$=i', $file, $mat)  &&  TV::is_tv($mat[1]));
  }



  // returns the number of seconds, may be fractional
  public static function runtime(Item $item, $nGroupsAV, $videofile) {
    $runtime = ($nGroupsAV<=1 ? $item->getMetaField('runtime') : null);

    if ($runtime) {
      if (DEBUG)
        msg($runtime);

      if (preg_match('/^[\d\.]+$/', $runtime)) {
        $runtime = $runtime; // eg: "60.1"
      } elseif (preg_match('/^(\d+):(\d+):([\d+\.]+)$/', $runtime, $m)) {
        $runtime = $m[1]*3600 + $m[2]*60 + $m[3];  // eg: "01:13:01"
      } elseif (preg_match('/^(\d+)\s*min ([\d+\.]+)\s*sec$/', $runtime, $m)) {
        $runtime = $m[1]*60 + $m[2];  // eg: "1min 13sec"
      } elseif (preg_match('/^(\d+):([\d+\.]+)$/', $runtime, $m)) {
        $runtime = $m[1]*60 + $m[2]; //eg: "13:01"
      } elseif (preg_match('/^(\d+)\s*minutes (\d+)\s*seconds/', $runtime, $m)) {
        $runtime = $m[1]*60 + $m[2]; //eg: "13 minutes 23 seconds"
      } elseif (preg_match('/^(\d+)\s*minutes/', $runtime, $m)) {
        $runtime = $m[1]*60; //eg: "13 minutes"
      } elseif (preg_match('/^(\d+)\s*seconds/', $runtime, $m)) {
        $runtime = $m[1]; //eg: "10 seconds"
      } else {
        $runtime = null;
      }
    }

    if (!$runtime) {
      return null; //xxxxxxxxxxxxxxxxxxxxxxxxxx too expensive!!

      $videofile = ('http://'.$item->getServer().dirname($item->getMainDir()) .
                    "/{$item->identifier}/$videofile");
      if (DEBUG)
        msg("have to fetch identify on $videofile...");

      if ($xxx = Video::identify($videofile)) {
        $runtime = $xxx['length'];
        if (DEBUG)
          msg($runtime);
      }
    }

    return $runtime;
  }


  public static function filesToGroupsAV(Item $item, $files, $prefix = '', $firstOnly = false, $onlyThisGroup = null) {
    $AUDIO = ['vbr mp3',
              'ogg vorbis',
              '128kbps mp3',
              '64kbps mp3',
              'mp3 sample',
    ];

    if (!Video::$PLAYABLES) {
      Video::$PLAYABLES = array_flip([
        'thumbnail2',//keeps all 'rank' values !0, also used for 2nd thumbnail..
        'thumbnail', //holder for 1st thumbnail

        // our current IA video derivatives:
        'h.264',
        'webm',
        'h.264 720p',
        'h.264 486p',

        // our prior IA video derivatives:
        '512kb mpeg4',
        'ogg video',

        // formats users might upload that we can play:
        'h.264 4k',
        'h.264 hd',
        'h.264 mpeg4',
        'mpeg4',
        'ogg theora',
        'flash video',

        // not messing with flakey VLC plugin (for now ;-)
        //'mpeg2',
        //'mpeg1',
        //'cinepack', // avi


        // FOR AUDIO!
        'vbr mp3',
        'ogg vorbis',
        '128kbps mp3',
        '64kbps mp3',
        'mp3 sample',
      ]);
    }

    $HIRANK = 100;


    // cluster original files with their derivatives
    $groups= [];
    $audio = [];
    $vtts  = [];
    $srts  = [];
    $durations = [];
    $heights = []; // for SD/HD toggling!
    $widths  = []; // for SD/HD toggling!
    $autoplay= [];
    $lengths = []; // used to 'prefer' the <length> in deriv <file>
    $artists = []; // someone please shoot me

    $t2 = Video::$PLAYABLES['thumbnail2'];

    $groupsA = [];
    $groupsV = [];
    $haxLP = false;
    foreach ($files as $filocation => $fi) {
      $short = ltrim($filocation, '/');
      $formatLC = strtolower($fi['FORMAT']);
      $suffixLC = strtolower(strrchr($short, '.'));

      if ($formatLC == 'item tile'  ||  $short === Imager::THUMB_NAME)
        continue; // ignore these files altogether

      if ($fi['ORIGINAL']=="{$item->identifier}_segments.xml"  &&  $suffixLC=='.mp3') {
        // pretend this mp3 is an original and not "derived from"
        // an XML file...
        $haxLP=true;//you think i $&*@# like this?  thx4gun2head...
        unset($fi['ORIGINAL']);
        $fi['SOURCE']='original';
        //printr($fi);
      }

      if ($suffixLC == '.vtt') {
        $vtts[] = $short; // save for later
        continue;
      }

      if ($suffixLC == '.srt') {
        $srts[] = $short; // save for later
        continue;
      }

      $rank = Video::$PLAYABLES[$formatLC];
      if ($suffixLC=='.mp3'  &&  !$rank) {
        // OK some items get super specific on the bitrates, like "32Kbps MP3" eg:
        //    /details/Dragnet_OTR
        // so for them, let's pretend they are like less-preferred MP3 <format>
        // so they'll actually show up in the A/V player, etc....
        $formatLC = '64kbps mp3';
        $rank = Video::$PLAYABLES[$formatLC];
      }

      if (isset($fi['ORIGINAL'])) {
        // derivative!
        $deriv = true;
        $key = $fi['ORIGINAL'];

        if (($suffixLC=='.mp4' || $suffixLC=='.ogv')  &&
            isset($files["/$key"])  &&
            ('ISO Image' == $files["/$key"]['FORMAT'])  &&
            preg_match('/^(.*?)(\d+)\.[^\.]+$/', $short, $m2)  &&
            preg_match('/^(.*)\.[^\.]+$/', $key, $mat)  &&
            $m2[1]==$mat[1]) {
          // Yikes, OK for deriving DVD disc images, we can make
          //   file.cdr => file.mp4, file1.mp4, file2.mp4
          // when there are *3* titles/tracks in the DVD.
          // BUT each show the same "original file".
          // So, since we think we've detected this case w/ the 'if' above,
          // we'll fudge the "original file"(name) a bit by adding back
          // in any numerical track info, so that we can make the A/V player
          // show/play all "tracks/titles"!
          //if (DEBUG) msg("{$fi['ORIGINAL']} vs $short ({$mat[1]} vs {$m2[1]})");
          $origSuffix = strrchr($fi['ORIGINAL'], '.');
          $key = "{$m2[1]}{$m2[2]}$origSuffix";
        }


        if (is_array($key))
          $key = (count($key) ? $key[0] : '');
      } else {
        // original file!
        $deriv = false;
        $key = $short;
        if (isset($fi['ARTIST']))
          $artists[$key]=$fi['ARTIST'];
      }

      if ($onlyThisGroup  &&  $key!=$onlyThisGroup)
        continue;

      if (!$deriv) {
        // use file's explicit <title> if it exists;
        // else (later, base (no subdirs) filename, w/o final suffix)
        $name = $fi['TITLE'];
        $groups[$key]['TITLE'] = $name;
      } else {
        // derivative -- use any title it has as backup/2nd choice
        // for example an unlogged in user viewing a Grateful Dead
        // soundboard stream-only item -- they never see the original
        // in the file loop iteration we are in!
        if ($fi['TITLE']  &&  !isset($groups[$key]['TITLE']))
          $groups[$key]['TITLE'] = $fi['TITLE'];
      }

      if (isset($fi['HEIGHT']))
        $heights[$short] = $fi['HEIGHT'];
      if (isset($fi['WIDTH']))
        $widths[$short] = $fi['WIDTH'];

      if (isset($fi['AUTOPLAY'])) {
        // NOTE: this is _usually_ used to say "stop at this point in playlist", eg:
        // /details/78_a-special-collection-of-the-world-famous-music-of-trinidad---calypsos_wilmoth-houdi_gbia0002481
        $autoplay[$short] = $fi['AUTOPLAY'];
      }

      if (isset($fi['LENGTH'])) {
        // convert to number of seconds
        $sec = $fi['LENGTH'];
        if (preg_match('/^(\d+):(\d+)$/', $sec, $mat))
          $sec = (60*$mat[1])+($mat[2]);
        elseif (preg_match('/^(\d+):(\d+):(\d+)$/', $sec, $mat))
          $sec = (3600*$mat[1])+(60*$mat[2])+($mat[3]);

        $durations[$short] = $sec;

        // shn file durations seem wacky, eg:
        //   /details/bdolzani2011-06-28
        // so we'll 'prefer' derivs' LENGTH when we can...
        if ($deriv)
          $lengths[$key]=1; // set to indicate derivative set the LENGTH

        if ($deriv  ||  !isset($groups[$key]['LENGTH'])  ||
            !isset($lengths[$key])) {
          $groups[$key]['LENGTH'] = round($sec);
        }
      }


      if (!$rank) {
        // not a playable file, skip it -- now that we've looked for
        // its TITLE and/or saved it ;-)
        continue;
      }


      if ($deriv) {
        if ($formatLC=='h.264 720p') {
          $groups[$key]['HD'] = $short;
          continue;
        }
      } else {
        // tracey = lame. Court submits exhibit #2 on behalf of The People vs..
        // We **LOVE** this item/movie!
        //    http://archive.org/details/Sita_Sings_the_Blues
        // but it has a large number of h.264 video <format> files in it
        // and we needed someway to filter out monstrously large 'HD' ones...
        if ($formatLC=='h.264 hd'  ||
            // eg: archive.org/details/bancroftlibraryucberkeley
            ($formatLC=='h.264 mpeg4'  &&  (strpos($name, '[HD]')  ||
                                            strpos($name, ' HD ')  ||
                                            preg_match('/ HD$/', $name)))) {
          $groups[$key]['HD'] = $short;
          continue;
        }

        if ($formatLC=='h.264 4k') {
          $groups[$key]['4K'] = $short;
          continue;
        }

        if ($rank > 1)
          $rank += $HIRANK; // deprioritize non-derivatives!
      }


      if ($rank > 1) {
        // ie: not thumbnail
        if (in_array($formatLC, $AUDIO)) {
          $groupsA[$key] = 1;
          $audio[$short] = 1;
        } else {
          $groupsV[$key] = 1;
        }
      }

      if (!isset($groups[$key][$rank])) {
        $groups[$key][$rank] = $short;
        /**/ if ($rank < $HIRANK  &&  isset($groups[$key][$rank + $HIRANK]))
          unset($groups[$key][$rank + $HIRANK]);
        elseif ($rank > $HIRANK  &&  isset($groups[$key][$rank - $HIRANK]))
          unset($groups[$key][$rank - $HIRANK]);
      } elseif ($formatLC=='thumbnail'  &&  !isset($groups[$key][$t2])) {
        $groups[$key][$t2] = $short;
      }
    }
    ksort($groups);

    if ($haxLP) {
      // yank "entire album" tracks from being shown in A/V player
      unset($groups["{$item->identifier}.flac"]);
      unset($groups["{$item->identifier}.ogg"]);
      unset($groups["{$item->identifier}.mp3"]);
    }


    $nAudio = count($groupsA);
    $nVideo = count($groupsV);
    $showing = (($nVideo > 0  &&
                 ($nVideo >= $nAudio ||
                  $item->getMetaField('mediatype')=='movies')) ? 'movies' :
                ($nAudio ? 'audio' : false));

    // we only care if 2+ (different!) <artist> values are set
    //   eg:  /details/wcd_various-artistsbravo-hits-24_2-4-family_flac_lossless_29950093
    if (count(array_flip($artists)) <= 1)
      $artists = false;

    if (DEBUG) {
      msg('DURATIONS');
      printr($durations);
      msg('HEIGHTS');
      printr($heights);
      msg('WIDTHS');
      printr($widths);
      msg('ARTISTS');
      printr($artists);
      msg('PLAYABLES');
      printr(Video::$PLAYABLES);
      msg('GROUPS');
      printr($groups);
      msg('groupsAV (a:' . count($groupsA) . ',v:' . count($groupsV) . ') audio:');
      printr($audio);
    }
    $groupsAV = ($showing == 'movies' ? $groupsV : $groupsA);


    $grouped = [];
    foreach ($groups as $orig => $rankToFile) {
      if (!isset($groupsAV[$orig]))
        continue;

      $kv = [];

      ksort($rankToFile);

      if ($rankToFile[0])
        $kv['POSTER'] = $prefix.$rankToFile[0];
      elseif ($rankToFile[1])
        $kv['POSTER'] = $prefix.$rankToFile[1];
      else {
        // ugh, this is terrible -- try to find files named 'FILE.png' that correspond
        // to mp3/ogg files that aren't otherwise found above.  This is so we can group the
        // waveforms PNGs (and, BTW _not_ spectrogram PNG) into the A/V file group.
        foreach ($rankToFile as $keykey => $valval) {
          if (is_numeric($keykey)) {
            $tmp = preg_replace('/(\.ogg|_sample\.mp3|\.mp3)$/i', '.png', $valval);
            if ($tmp != $valval  &&  $files["/$tmp"]) {
              $kv['POSTER'] = $prefix.$tmp;
            }
          }
        }
      }

      if (isset($autoplay[$orig]))
        $kv['AUTOPLAY'] = filter_var($autoplay[$orig], FILTER_VALIDATE_BOOLEAN);


      foreach ($rankToFile as $rank => $src) {
        if (is_numeric($rank)  &&  $rank>1) {
          // not a POSTER or TITLE!
          if (($showing=='movies'  &&   $audio[$src])  ||
              ($showing!='movies'  &&  !$audio[$src])) {
            continue;
          }

          $kv['SRC'][] = $prefix.$src;
        }
      }

      if (!isset($kv['SRC']))
        continue; // no derivatives in video (or audio) group of files, likely!



      $name = $rankToFile['TITLE'];
      if (!$name) {
        $name = str_replace('_', ' ', preg_replace(
          '/_512kb$/',
          '',
          preg_replace('/\.[^\.]+$/', '', basename($orig)) //nix suffix
        ));
      }
      if ($artists  &&  isset($artists[$orig]))
        $name .= " - {$artists[$orig]}";
      $kv['TITLE'] = $name;


      $kv['ORIG']  = $orig;
      if (isset($rankToFile['LENGTH'])) {
        $kv['LENGTH'] = $rankToFile['LENGTH'];
      } else {
        // this *could* get a bit expensive esp. for items w/ tons of videos!!
        // xxxx should cache this & entire HTML from /details/ page for X mins...
        $kv['LENGTH'] = Video::runtime($item, count($groupsAV), $orig);
      }
      if (isset($rankToFile['HD']))
        $kv['HD'] = $rankToFile['HD'];
      if (isset($rankToFile['4K']))
        $kv['4K'] = $rankToFile['4K'];

      $grouped[] = $kv;
    }

    if (DEBUG) {
      msg('filtered final set:');
      printr($grouped);
    }



    $captions = ($showing == 'movies' ?
                 Video::captions($grouped, $vtts, $srts, $item->identifier) : []);

    return[($firstOnly ? $grouped[0] : $grouped),
           count($groupsAV),
           $showing,
           $captions,
           $durations,
           $heights,
           $widths];
  }


  // setup subtitles/captions
  private static function captions($groups, $vtts, $srts, $identifier) {
    if (!count($vtts)  &&  !count($srts))
      return [];

    $captions = [];
    $captioned = false;

    foreach ([$srts, $vtts] as $fis) {
      if (DEBUG)
        printr($fis);

      foreach ($fis as $fi) {
        // given semi-arbitrary names like:
        //   "movie.vtt"
        //   "track1.srt"
        //   "track1.fr.vtt"
        //   "track1.sp.srt"
        // try to match it to the most appropriate video clip
        $base1 = substr($fi, 0, strlen($fi) - 4); // nix .vtt/.srt suffix
        $base2 = preg_replace('/[_\-\.][^_\-\.]+$/', '', $base1);

        $lang = '';
        if ($base2 != $base1) {
          // save the likely 'language' portion we just found
          $lang = strtolower(substr($base1, strlen($base2)+1));
          // vtt:   track1.something.vtt
          // srt:   track1.something.srt
          // base1: track1.something
          // base2: track1
          // lang:  something
        }
        //msg("$fi --- $base1 --- $base2 --- $lang");

        // now search the videos to see what it matches best to (if any)
        $bestmatch1 = '';
        $bestmatch2 = '';

        for ($idx=0; $idx < count($groups); $idx++) {
          $clip = $groups[$idx]['ORIG'];

          // remove any suffix
          $vidname = preg_replace('/(_512kb)*\.[^\.]+/', '', $clip);
          //printr("($idx) [$vidname] vs:");
          //printr([$base1, $base2]);

          if ($vidname == $base1)  $bestmatch1 = $idx;
          if ($vidname == $base2)  $bestmatch2 = $idx;
        }

        if /**/($bestmatch1 !== '') $vidN = $bestmatch1;//NOTE: prefer longest match!
        elseif ($bestmatch2 !== '') $vidN = $bestmatch2;
        elseif (count($groups)==1) $vidN = 0;//only *1* video track -- match!
        else continue;

        if (DEBUG)
          printr("bestmatch1:$bestmatch1 --- bestmatch2:$bestmatch2");

        if (!isset($captions[$vidN]))
          $captions[$vidN] = [];


        if (!$lang) {
          // default to 'en' if no language found above and it hasnt been 'used' yet
          if (!isset($captions[$vidN]['en'])) {
            $lang = 'en';
          } else {
            // basically back off / give up and use the filename
            $lang = basename($fi);
            $lang = substr($lang, 0, strlen($lang) - 4); // nix .vtt/.srt suffix
          }
        }



        $captions[$vidN][$lang] = "/download/$identifier/$fi";

        //printr("BESTMATCH: $bestmatch1 $bestmatch2 $vidN $lang");
        //printr($captions);

        $captioned = true;
      }
    }
    //printr($captions);

    if (!$captioned)
      return [];

    return $captions;
  }



  // sanity check the overall identified length/duration of videos, adjusting as needed
  private static function adjust_length($file, &$arr, $tvarchive, $use_packets_length = false) {
    // sanity check the overall bytes/sec!  (see if the 'length' is waaaaay off)
    // blu-ray is unlikely to ever max > 64 Mbit/s video, for example...
    // ( 64 Mbit/s == 66536 Kbit/s == 67108864 bit/s )
    $kbs=0;
    if ($arr['bytes']>0  &&  ($arr['length']==0  ||  ($arr['length']!=0  &&
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
    $dir=dirname(realpath($file));
    $id=basename($dir);
    if (!($xml = @simplexml_load_file("{$dir}/{$id}.xml")))
      return;

    if (($sec = $arr['length'])  &&
        isset($xml->DURATION)  &&
        count($xml->DURATION)  &&
        (preg_match('/^(\d+):(\d+):(\d+)$/', $xml->DURATION[0], $mat))  &&
        ($duration = $mat[1]*3600 + $mat[2]*60 + $mat[3])) {
      $toolong  = ($sec-$duration > TV::ALLOWED_LONGER);
      $tooshort = ($duration-$sec > TV::ALLOWED_SHORTER);

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
        implode($lines, "\n  # ").
        "\n  ##################  CLIP INFO  ################ \n";
    }

    $out = implode($lines, "\n");




    $width  = null;
    $height = null;
    $video = '';
    $vline = self::pick_vstream($file, $out);
    if (preg_match('/^(Stream .*? Video: .*?, (\d+)x(\d+)(, | \[)(.*))/', $vline, $mat)) {
      $width = $mat[2];
      $height= $mat[3];
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
      'bytes'   => (strncmp($file, 'http://', 7)==0 ? 0 : @filesize($file)),
      'length' => (preg_match(
                    '/\n *Duration: (\d+):(\d+):([\d\.]+)/',
                    $out,
                    $mat
                  ) ? ((60 * 60 * $mat[1]) + (60 * $mat[2]) + $mat[3]) : null),

      // example (wily+):
      //   built with gcc 5.2.1 (Ubuntu 5.2.1-21ubuntu2) 20151003
      'ffmpeg-version' => (preg_match('/\n  built with .*? \([^\)]+\) (\d{8})\n/', $out, $mat) ? $mat[1] : ''),
    ];
    if ($arr['ffmpeg-version']=='20151010')
      $arr['ffmpeg-version']   ='20160116'; // wack! took *gcc* build time?!




    Video::adjust_length($file, $arr, $tvarchive, $use_packets_length);


    if (strpos($out, 'multiple edit list entries, a/v desync might occur, patch welcome')>0)
      $arr['vsync'] = 1;


    // For TV archive items, we want to always use the first audio track found
    // Save 2 streams like: "0.1,0.2"
    if (preg_match_all('/ Stream \#(\d+.\d+).*?(: Audio: .*)/', $out, $alines)) {
      // should we prioritize 5.1 channels first?  yes, unless like TV but *not* HD
      $mpegts = (strpos($out, 'Input #0, mpegts, from ')!==FALSE);
      $HDTV = ($mpegts  &&  $arr['height'] >= 720);
      $prefer51 = (!$mpegts || $HDTV);

      if ($tvarchive  &&  !$prefer51) {
        // uho!  KNTV started uploading SD material with *only* good audio in *5.1*!  (feb2014)
        // eg: /details/KNTV_20140211_123000_Today_in_the_Bay
        // So did KGO starting about here: /details/KGO_20140710_030000_The_Middle
        list($begTS, $ch) = Util::nofatal(function () use ($file) {
          return TV::identifierTS2(basename($file));
        });
        if (($ch=='KNTV'  &&  $begTS >= 1391212800)  || // strtotime('2014-02-01')
            ($ch=='KGO'   &&  $begTS >= 1404961200)  || // strtotime('2014-07-10')
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
      for ($lineN=0; $lineN < count($alines[0]); $lineN++) {
        $aline    = $alines[0][$lineN];
        $streamID = $alines[1][$lineN];

        // oneiric era + goes from/to:
        // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
        // Audio: mp1 ([6][0][0][0] / 0x0006), 48000 Hz, stereo, s16, 416 kb/s
        $aline = preg_replace('= \(\[\d+\]\[\d+\]\[\d+\]\[\d+\] / 0x\d+\)=', '', $aline);

        if (count($alines[0])==3  &&  $tvarchive) {
          // throw out likely misdetected MP3 stereo lines (used to be wackier (below) until trusty...)
          if (preg_match('=: Audio: mp3, .*?, stereo, =', $aline))
            continue;
        }


        if (count($alines[0])==3  &&
            // throw out suspected-decoded-wrong audio streams like:
            // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
            // /details/WUSA_20110519_223000_CBS_Evening_News_With_Katie_Couric
            (strpos($aline, ': Audio: mp1,')>0  ||
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091019_160000_9News_Now_at_Noon
             strpos($aline, ': Audio: aac, 24000 Hz, 7.1,')>0  ||  //jan2014 version
             strpos($aline, ': Audio: aac, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)')>0  || //sep2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)')>0  || //mar2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 7.1(wide)')>0  || //nov2011 version
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091118_140000_9News_Now_at_9am    -- it should be noted this item is botched/glitchy
             strpos($aline, ': Audio: aac, 7.1, fltp, ')>0  || //jan2014 version
             strpos($aline, ': Audio: aac, 3.0, fltp, ')>0  || //dec2012 version
             strpos($aline, ': Audio: aac, 3.0, s16, ')>0  || //sep2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)')>0  || //mar2012 version
             strpos($aline, ': Audio: aac, 7350 Hz, 3 channels')>0 )) { /*nov2011 version*/
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
          if ($m[1]==0) {
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
          $arr['program']=$m[1];
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
      if ($arr['rotation']==90  ||  $arr['rotation']==270) {
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
    if (count($vlines[1])==1)
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
      if (strncmp(Strings::killspace($tmp), $vstream, strlen($vstream))===0) {
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

    if ($identify['vc']!='h264'  ||
        ($identify['ac']!='aac'  &&  $identify['audio']!=='false')  ||
        !Util::starts_with($identify['pix'], 'yuv420p')) {
      if ($verbose)
        error_log(print_r($identify, 1));
      return false;
    }

    if ($max_bitrate) {
      $bitrate = round($identify['bytes'] / max(1, $identify['length']) *  8 / 1024);
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
    $hr  = floor($seconds/3600);
    $min = floor(($seconds - ($hr*3600))/60);
    $sec = $seconds - ($hr*3600) - ($min*60);
    if (!$nDecimals  &&  $tenthSecond)
      $nDecimals = 1;
    $secfmt = ($nDecimals ? '%04.' . $nDecimals . 'f' : '%02d');
    $ret = ($omit00hr  &&  $hr==0 ?
            sprintf("%02d:$secfmt", $min, $sec) :
            sprintf("%02d:%02d:$secfmt", $hr, $min, $sec));

    if ($nDecimals  &&  $sep !== '.')
      $ret = str_replace('.', $sep, $ret);

    return $ret;
  }


  public static function hms2sec($hms, $fractional = false) {
    if ($fractional && preg_match('/^(\d+):(\d+):([\d+\.]+),(\d+)$/', $hms, $m)) //eg: "01:13:01,939"
      return $m[1]*3600 + $m[2]*60 + $m[3] + floatval(".{$m[4]}");
    if (preg_match('/^(\d+):(\d+):([\d+\.]+)$/', $hms, $m)) //eg: "01:13:01"
      return $m[1]*3600 + $m[2]*60 + $m[3];
    if (preg_match('/^(\d+):([\d+\.]+)$/', $hms, $m)) //eg: "13:01"
      return $m[1]*60 + $m[2];
    if (preg_match('/^([\d+\.]+)$/', $hms, $m)) //eg: '01'
      return intval($m[1]);
    return false;
  }


  public static function hhmmss2sec($hhmmss) {
    if (!preg_match('/^(\d\d)(\d\d)(\d\d)$/', $hhmmss, $m))
      return false;

    return self::hms2sec("{$m[1]}:{$m[2]}:{$m[3]}");
  }


  public static function sec2ms($seconds) {
    $runtime = '';
    if ($seconds > 60) {
      $runtime .= ' '.floor($seconds / 60).' minutes';
      $seconds = fmod($seconds, 60);
    }
    if ($seconds)
      $runtime .= ' '.round($seconds).' seconds';
    return trim($runtime);
  }



  private static function tofloat($rate) {
    if (intval($rate)==0)
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

    $fps=-666;
    if (preg_match('/, ([\d\.]+) fps,/', $vline, $mat))
      $fps = $mat[1]; // fallback -- we can usually be more accurate than this!!

    $ffprobe = Config::petaboxPath('bin-ffprobe').(Video::file_is_tv($src) ? ' -f mpegts ':'');
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
      if ($ary['codec_type']=='video') {
        // If not really parseable stream, we'll effectively note that for later..
        if ($ary['width']!=='0')
          $goods[] = $ary;

        $vstreams[] = $ary;
      }

      if ($ary['codec_type']=='audio')
        $astreams[] = $ary;
    }

    if (count($vstreams) > 1  &&  count($goods) > 0  &&
        count($vstreams) > count($goods)) {
      // throw out any video stream whose dimensions cant be parsed!
      // eg: /details/FOXNEWS_20091125_020000_Hannity
      $vstreams = $goods;
    }


    if ($vstream!==''  &&  count($vstreams) > 1  &&  count($goods)==count($vstreams)) {
      foreach ($goods as $good) {
        if ($good['index']===preg_replace('/^0\:/', '', $vstream)) {
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

          if (intval($rate)!=0) {
            // take the 1st one found (almost always 'r_frame_rate')

            // check for wack-ness
            // Eg: quite a few ".m4a" files which seem to be mostly m4a audio
            // but have a video motion-JPEG frame very very infrequently.
            // anyway, they aren't no 600 fps!!
            if ($rate=='600/1'  ||  // /details/Podcast7_492
                $rate=='600/2094059') { // /details/OvercomePt.3-OvercomeTheWorldWithLove_787
              $rate = '6/1';
            } elseif (($rate=='60000/1001'  ||  $rate=='359/6')  &&
                     $stream['codec_time_base']=='1/2997') {
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
    $HD = ($tv  &&  $identify['height']>=720);
    $maxtime = Util::timeoutTime($length, ($HD ? '6x' : '1.5x'), ($HD ? '4h' : '2h'), $identify['bytes']);


    // Some items have so much damage they can fail deep into the
    // extraction and segfault ffmpeg when deinterlace-ing, eg:
    //    /details/TELECONGO_20110812_190000
    // so try failures one more time w/o the deinterlacing...
    $tries = [];
    $tries[] = $cmd;
    $tries[] = Video::yadif_nix(/**/                              $tries[count($tries)-1]);
    $tries[] = str_replace('setpts=PTS-STARTPTS,', '',            $tries[count($tries)-1]);
    $tries[] = str_replace(' -i ', ' -err_detect 0 -i ',          $tries[count($tries)-1]);
    $tries[] = str_replace(' -i ', ' -fflags discardcorrupt -i ', $tries[count($tries)-1]);
    // last try is severe, but some items have *SO* much packet damage, we'd rather get
    // any images we can at all, eg: /details/WPVI_20140803_090000_Puerto_Rican_Panorama
    $tries = array_values(array_unique($tries)); // nix dupes; retains order!
    //printr($tries);

    foreach ($tries as $idx => $cmd) {
      // TV archive has special behaviour if last attempt fails...
      $last = ($idx==(count($tries)-1));
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
      $thumbnails = array_slice($thumbnails, 1, $maxThumbs+1);
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
          if ($i==0) {
            $sec = 1;
          } else {
            $sec = intval(basename($thumbnail));//newer version, named based on seconds
            if ($sec < 0)
              $sec = round($i*$frameEvery);
          }
        }

        $num = ($timestamps ? $sec : $i+1);

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


  public static function yadif_nix($str) {
    return str_replace('yadif=0:-1:1,', ' ', str_replace('yadif,', '', $str));
  }
  public static function vf_nix($cmd, $vf) {
    $vf = (array)$vf;
    foreach ($vf as $str)
      $cmd = str_replace("$str,", '', $cmd);
    return $cmd;
  }


  const VF_PTS    = 'setpts=PTS-STARTPTS';
  const VF_YADIF  = 'yadif';
  const VF_SQUARE = "'scale=trunc(iw*sar/2)*2:trunc(ih/2)*2'";
  const VF_SAR    = 'setsar=1/1';
  const VF_TOP4   = 'crop=iw:ih-4:0:4';

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
      $filters[] = "'select=eq(pict_type\,I)'"; // xxx iframes only -- but _wanted_ gap/spacing..

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
    $traction_control = ($identify['audio']!=='false'  &&
                         ($tvarchive  ||  (isset($identify['vsync'])  &&  $identify['vsync']==1)  ||
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
        $monosDV = ((@$identify['vc']=='dvvideo'  ||
                     @$identify['vc']=='v210')  &&  // eg: /details/XFR_2013-09-07_1B_01
                    ((@$identify['audio_nchannels']==2  &&  count(@explode(',', @$identify['astreams']))==2)  ||
                     (@$identify['audio_nchannels']==4  &&  count(@explode(',', @$identify['astreams']))==4)));
        if ($monosDV) {
          echo "\nWARNING! DV MONO 2/4 CHANNELS DETECTED!  DOWNMIX ALL TO MONO!\n\n";
          $map .= ' -filter_complex "[0]amerge[aout]" -map "[aout]" -ac 1 ';
        } else {
          // The 2nd specifier (after comma) is for 'sync_file_id'
          // and *both* should indeed be using the *vid*
          $map .= ' -map '.$aid.($traction_control ? ",$vid" : '');
        }

        $acoder = "$map $acoder";
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
    // For MPEG-TS, sometimes the pts 'clock' can 'rollover' and cause us to fail.
    // So to workaround, reset pts to start at '0' and ever increase.
    // Eg: head -20000000 /details/../BBC2_20121219_232000_Stuffed__The_Great_British_Christmas....mpg
    // nicely shows the "clock rollover" (and this worked around failed .mp4 creation!)
    if (!strpos($tries[count($tries)-1], self::VF_PTS))
      $tries[] = str_replace(' -vf ', ' -vf '.self::VF_PTS.',', $tries[count($tries)-1]);
    if ($traction_control)
      $tries[] = $tries[count($tries)-1] . $rate;
    $tries[] = self::vf_nix($tries[count($tries)-1], self::VF_YADIF);
    $tries[] = str_replace(' -i ', ' -err_detect 0 -i ', $tries[count($tries)-1]);

    // /details/ALMAGHRIBIA_20190116_200000
    $tries[] = self::vf_nix($tries[count($tries)-1], [self::VF_SQUARE, self::VF_SAR, self::VF_TOP4]);


    $tries = array_values(array_unique($tries)); // nix dupes; retains order!
    //printr($tries);

    return [$tries, $acoder];
  }


  // start at first I-frame (eg: MPEG-TS and MPEG-PS)
  public static function seek_to_first_keyframe($cmdpipe, $seekMaxB = 1000000) {
    $ffprobe = Config::petaboxPath('bin-ffprobe');

    // NOTE: make sure that we skip any "first keyframe" that (bizarrely) comes up
    // a **negative** byte position.
    // @codingStandardsIgnoreLine
    $first_byte_for_first_keyframe = Util::cmd("( $cmdpipe | head -c $seekMaxB | $ffprobe -print_format compact -show_frames -select_streams v -show_entries frame=pict_type,pkt_pos - 2>/dev/null |fgrep -m1 pict_type=I |egrep -o 'pos=[0-9]+'|cut -f2 -d= ) 2>/dev/null", 'CONTINUE', 'PRINT');

    return ($first_byte_for_first_keyframe ?
            " | tail -c +$first_byte_for_first_keyframe " : '');
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
      if ($metaName=='date'  &&  $pre=='-metadata year=') {
        // special case extra needed check to make sure it's a 4-digit number!
        if (!preg_match('/^\d\d\d\d$/', self::$videoMetaCache['date'][0]))
          continue;
      }

      // append /details/ url to the title, if desired (and not already the title)
      $valpost = (($addurl  &&  $metaName == 'title'  &&
                   self::$videoMetaCache['title'][0] != $url) ? " - $url" : '');

      // try to get any **<file> tag specific** <title> element first.  if none,
      // get the overall item's <title>
      if ($tmp=self::metadataString($module->metadata, $metaName, $pre, '', $valpost)) {
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

    $tests = [
      // simple case
      '{"identifier":"something","filesInfo":{
"boo.avi":{"FORMAT":"Cinepack",
           "LENGTH":6,
           "TITLE":"avi title"},
"boo.mp4":{"FORMAT":"h.264",
           "LENGTH":6,
           "ORIGINAL":"boo.avi"}
}}' =>
      '[{"SRC":["boo.mp4"],"TITLE":"avi title","ORIG":"boo.avi","LENGTH":6}]',


      // simple case
      '{"identifier":"something","filesInfo":{
"boo.avi":{"FORMAT":"Cinepack",
           "LENGTH":6,
           "TITLE":"avi title"},
"boo.mp4":{"FORMAT":"h.264",
           "LENGTH":6,
           "ORIGINAL":"boo.avi"},
"boo.webm":{"FORMAT":"WebM",
           "LENGTH":6,
           "ORIGINAL":"boo.avi"}
}}' =>
      '[{"SRC":["boo.mp4","boo.webm"],"TITLE":"avi title","ORIG":"boo.avi","LENGTH":6}]',


      // very tough (but concise!) case where the orig *and* derivative file
      // are the same <format> and we want to ensure that we *only* get returned
      // the derivative...
      '{"identifier":"something","filesInfo":{
"boo.mov":{"FORMAT":"h.264",
           "LENGTH":6,
           "TITLE":"boo mov title"},
"boo.mp4":{"FORMAT":"h.264",
           "LENGTH":6,
           "ORIGINAL":"boo.mov"}
}}' =>
      '[{"SRC":["boo.mp4"],"TITLE":"boo mov title","ORIG":"boo.mov","LENGTH":6}]',
    ];


    foreach ($tests as $test => $shouldB) {
      $meta = json_decode($test, 1);
      $item = new Item($meta['identifier']);
      $item->meta = $meta;
      $item->filesInfo = $meta['filesInfo'];
      $meta['mainDir'] = '/';
      foreach ($meta['filesInfo'] as $filocation => &$fi)
        $fi = (array)$fi;
      unset($fi);

      list($groups, $nGroupsAV, $showing, $captions)=self::filesToGroupsAV($item, $meta['filesInfo']);
      printr($groups);
      assert(json_encode($groups) == $shouldB);
    }

    assert(Video::hms(13.3)              == '00:00:13');
    assert(Video::hms(13.3, 1)           == '00:13');
    assert(Video::hms(13.3, 1, 1)        == '00:13.3');
    assert(Video::hms(13.3, 0, 1)        == '00:00:13.3');
    assert(Video::hms(13.9, 0, 1)        == '00:00:13.9');
    assert(Video::hms(13.9, 0, 0, 3)     == '00:00:13.900');
    assert(Video::hms(13.9, 0, 1, 3)     == '00:00:13.900');
    assert(Video::hms(13.9, 0, 1, 3, '.')== '00:00:13.900');
    assert(Video::hms(13.9, 0, 0, 3, ',')== '00:00:13,900');

    assert(Video::hhmmss2sec('021304') == 7984);


    if (!$all)
      return;


    // rectangular pixel, aspect ratio, scale and cropping tests
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
        'width'  => ($conf[0]==='w' ? preg_replace('/[^\d]/', '', $conf) : -2),
        'height' => ($conf[0]==='h' ? preg_replace('/[^\d]/', '', $conf) : -2),
      ];
      printr($ary);
      printr($args);
      $vf = self::vf($args);

      Util::cmdPP("convert -size $wh xc:black frame.png");
      Util::cmdPP("/petabox/bin/ffmpeg -y -v 0 -r 1 -i frame.png -vf setsar=$sar,setdar=$dar in.mp4");
      Util::cmdPP("/petabox/bin/ffmpeg -y -v 0 -i in.mp4 $vf out.mp4");

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
    foreach (['/home/tracey/petabox', '/petabox'] as $pethome) {
      $ffprobe = "$pethome/" . Config::value('bin-ffprobe');

      $head20 = ['-print_format compact -select_streams v -show_packets'=>1];
      foreach (['-print_format compact -select_streams v -show_packets' =>
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
                ['{"streams": [{"index": 0,"codec_name": "mpeg2video","codec_long_name": "MPEG-2 video","profile": "Main","codec_type": "video","codec_time_base": "1001/60000","codec_tag_string": "[2][0][0][0]","codec_tag": "0x0002","width": 528,"height": 480,"has_b_frames": 1,"sample_aspect_ratio": "40:33","display_aspect_ratio": "4:3","pix_fmt": "yuv420p","level": 8,"timecode": "10:40:00;12","id": "0x880","r_frame_rate": "30000/1001","avg_frame_rate": "30000/1001","time_base": "1/90000","start_pts": 8286398944,"start_time": "92071.099378","duration_ts": 162162,"duration": "1.801800","disposition": {"default": 0,"dub": 0,"original": 0,"comment": 0,"lyrics": 0,"karaoke": 0,"forced": 0,"hearing_impaired": 0,"visual_impaired": 0,"clean_effects": 0,"attached_pic": 0}}]}',
                '{ "streams": [ { "index": 0, "codec_name": "mpeg2video", "codec_long_name": "MPEG-2 video", "profile": "Main", "codec_type": "video", "codec_time_base": "1001/60000", "codec_tag_string": "[2][0][0][0]", "codec_tag": "0x0002", "width": 528, "height": 480, "has_b_frames": 1, "sample_aspect_ratio": "40:33", "display_aspect_ratio": "4:3", "pix_fmt": "yuv420p", "level": 8, "color_range": "tv", "timecode": "10:40:00;12", "id": "0x880", "r_frame_rate": "30000/1001", "avg_frame_rate": "30000/1001", "time_base": "1/90000", "start_pts": 8286398944, "start_time": "92071.099378", "duration_ts": 165165, "duration": "1.835167", "max_bit_rate": "15000000", "disposition": { "default": 0, "dub": 0, "original": 0, "comment": 0, "lyrics": 0, "karaoke": 0, "forced": 0, "hearing_impaired": 0, "visual_impaired": 0, "clean_effects": 0, "attached_pic": 0 } } ] }'
                ],
                // @codingStandardsIgnoreEnd


                '-print_format compact -show_format -show_entries stream=codec_type,id' =>
                ['stream|codec_type=video|id=0x880
stream|codec_type=audio|id=0x881
format|filename=/home/tracey/public_html/_/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric.mpg|nb_streams=2|format_name=mpegts|format_long_name=MPEG-TS (MPEG-2 Transport Stream)|start_time=92070.357978|duration=2.543200|size=690148|bit_rate=2170959',//quantal
                 'program|stream|codec_type=video|id=0x880
stream|codec_type=audio|id=0x881

stream|codec_type=video|id=0x880
stream|codec_type=audio|id=0x881
format|filename=/home/tracey/public_html/_/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric/WJZ_20100122_000141_CBS_Evening_News_With_Katie_Couric.mpg|nb_streams=2|nb_programs=1|format_name=mpegts|format_long_name=MPEG-TS (MPEG-2 Transport Stream)|start_time=92070.357978|duration=2.576567|size=690148|bit_rate=2142845|probe_score=100',//trusty
                ],


                '-print_format compact -show_frames -select_streams v -show_entries frame=pict_type,pkt_pos' =>
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
                ['{"streams": [{"index": 0,"codec_name": "mpeg2video","codec_long_name": "MPEG-2 video","profile": "Main","codec_type": "video","codec_time_base": "1001/60000","codec_tag_string": "[2][0][0][0]","codec_tag": "0x0002","width": 528,"height": 480,"has_b_frames": 1,"sample_aspect_ratio": "40:33","display_aspect_ratio": "4:3","pix_fmt": "yuv420p","level": 8,"timecode": "10:40:00;12","id": "0x880","r_frame_rate": "30000/1001","avg_frame_rate": "30000/1001","time_base": "1/90000","start_pts": 8286398944,"start_time": "92071.099378","duration_ts": 162162,"duration": "1.801800","disposition": {"default": 0,"dub": 0,"original": 0,"comment": 0,"lyrics": 0,"karaoke": 0,"forced": 0,"hearing_impaired": 0,"visual_impaired": 0,"clean_effects": 0,"attached_pic": 0}},{"index": 1,"codec_name": "ac3","codec_long_name": "ATSC A/52A (AC-3)","codec_type": "audio","codec_time_base": "1/48000","codec_tag_string": "[129][0][0][0]","codec_tag": "0x0081","sample_fmt": "fltp","sample_rate": "48000","channels": 2,"bits_per_sample": 0,"dmix_mode": "-1","ltrt_cmixlev": "-1.000000","ltrt_surmixlev": "-1.000000","loro_cmixlev": "-1.000000","loro_surmixlev": "-1.000000","id": "0x881","r_frame_rate": "0/0","avg_frame_rate": "0/0","time_base": "1/90000","start_pts": 8286332218,"start_time": "92070.357978","duration_ts": 158400,"duration": "1.760000","bit_rate": "192000","disposition": {"default": 0,"dub": 0,"original": 0,"comment": 0,"lyrics": 0,"karaoke": 0,"forced": 0,"hearing_impaired": 0,"visual_impaired": 0,"clean_effects": 0,"attached_pic": 0}}]}',//quantal
                '{ "streams": [ { "index": 0, "codec_name": "mpeg2video", "codec_long_name": "MPEG-2 video", "profile": "Main", "codec_type": "video", "codec_time_base": "1001/60000", "codec_tag_string": "[2][0][0][0]", "codec_tag": "0x0002", "width": 528, "height": 480, "has_b_frames": 1, "sample_aspect_ratio": "40:33", "display_aspect_ratio": "4:3", "pix_fmt": "yuv420p", "level": 8, "color_range": "tv", "timecode": "10:40:00;12", "id": "0x880", "r_frame_rate": "30000/1001", "avg_frame_rate": "30000/1001", "time_base": "1/90000", "start_pts": 8286398944, "start_time": "92071.099378", "duration_ts": 165165, "duration": "1.835167", "max_bit_rate": "15000000", "disposition": { "default": 0, "dub": 0, "original": 0, "comment": 0, "lyrics": 0, "karaoke": 0, "forced": 0, "hearing_impaired": 0, "visual_impaired": 0, "clean_effects": 0, "attached_pic": 0 } }, { "index": 1, "codec_name": "ac3", "codec_long_name": "ATSC A/52A (AC-3)", "codec_type": "audio", "codec_time_base": "1/48000", "codec_tag_string": "[129][0][0][0]", "codec_tag": "0x0081", "sample_fmt": "fltp", "sample_rate": "48000", "channels": 2, "channel_layout": "stereo", "bits_per_sample": 0, "dmix_mode": "-1", "ltrt_cmixlev": "-1.000000", "ltrt_surmixlev": "-1.000000", "loro_cmixlev": "-1.000000", "loro_surmixlev": "-1.000000", "id": "0x881", "r_frame_rate": "0/0", "avg_frame_rate": "0/0", "time_base": "1/90000", "start_pts": 8286332218, "start_time": "92070.357978", "duration_ts": 161280, "duration": "1.792000", "bit_rate": "192000", "disposition": { "default": 0, "dub": 0, "original": 0, "comment": 0, "lyrics": 0, "karaoke": 0, "forced": 0, "hearing_impaired": 0, "visual_impaired": 0, "clean_effects": 0, "attached_pic": 0 } } ] }',//trusty
                ]
      ] as $args => $vals) {
        $pass = false;
        foreach ((array)$vals as $val) {
          $out = Util::cmd("$ffprobe $args $mpg 2>/dev/null ".($head20[$args]?' |head -20':''), 'PRINT');
          $orig = $out;
          if ($out==$val) {
            $pass=true;
            break;
          }

          if (substr(ltrim($val), 0, 1)=='{') {
            $val=json_decode($val, 1);
            $out=json_decode($out, 1);

            // changed when went to wily!
            unset($out['streams'][0]['coded_width']);
            unset($out['streams'][0]['coded_height']);
            unset($out['streams'][0]['chroma_location']);
            unset($out['streams'][0]['refs']);


            if ($out==$val) {
              error_log('JSON match!');
              $pass = true;
              break;
            }
          }
        }

        if (!$pass) {
          file_put_contents('/tmp/video-regress-shouldBe', print_r($val, 1));
          file_put_contents('/tmp/video-regress-actually', print_r($out, 1));
          $cmd = 'colordiff -U30 /tmp/video-regress-shouldBe  /tmp/video-regress-actually';
          error_log('NOT EQUAL!');
          echo $orig;
          error_log($cmd);
          passthru($cmd);
          assert(false);
        }
      }
    }


    // now test 'fps' frame rates
    // NOTE: some values are array of acceptable values where values can bounce between ffmpeg versions over time
    error_log('Checking frame rates');
    chdir('/home/tracey/public_html/_/') || fatal('no test vid dir!');
    foreach ([
    'Rage2012_201402/Rage2012.wmv' => 29.97,
    '007333/007333.asf' => 14.99,
    '06ConclusionDaAnterir/05ReflexionPonenciaJoseMurilo2.mp4' => 29.97,
    '06ConclusionDaAnterir/06ConclusionesDaAnterior..mp4' => 29.97,
    '070125_chantier/070125prisesext1fence.mov' => [10.16, 29.97],
    '170109perLaPalestina/170109corteopalestina.mov' => 25,
    'AAC-NON-MONOTONE/Xacti1010_trim2_test_2010_02_04.mp4' => 29.97,
    'AlcTestClip/AlcTestClip.mov' => 29.97,
    'Alivega427-BasicTraditionalPackage332/Alivega427-BasicTraditionalPackage332.mov' => 29.97,
    'AresQuarterlyProgressReport11/QPR11.m4v' => 29.97,
    'BayrampasaVaillantServisi444_1_494VaillantServisi_1000/8.flv' => 23.98,
    'Bij_de_Italiaan/Italiaan.3gp' => 15,
    'BraveNewFilmsFoxAttacksObama/foxattacksobama320.mov' => 29.97,
    'BrokeNProud/Bnp_finalCut_intro_explicit_720.mov' => 23.98, //NOTE: sep2012+ ffmpeg is now *not* ''...
    'BurningDowntheSuburbs/HoldenBurningSuburbs.mov' => 30,
    'CNNW_20110518_000000_In_the_Arena/CNNW_20110518_000000_In_the_Arena.mpg' => 29.97,
    'CNNW_20110601_130000_CNN_Newsroom/CNNW_20110601_130000_CNN_Newsroom.mpg' => 29.97,
    'CapeCodMarsh/CapeCodMarsh.avi' => 20,
    'CapeCodMarshClouds/CapeCodMarshClouds.avi' => 60,
    'ChicagoImmigrantRightsAndAnti-warMarch2/HPIM1832.MPG' => 24,
    'ChicagoImmigrantRightsAndAnti-warMarch2/HPIM1840.MPG' => 24,
    'Children_of_the_Trains_trailer/children_trailer.flv' => 29.97,
    'ChrisWJohnsonEasternscreechowlnestboxfootage26Apr2006506AMCDT/20060426050600feeding.mov' => 29.97,
    'DIA40/DIA40.mpg' => 29.97,
    'DNALOUNGE-2009-02-13/DNALOUNGE-2009-02-13.mp3' => '',
    'DennisPouletteWelcomeNathan/welcomenathanvlog.mov' => 15,
     // @codingStandardsIgnoreStart
    'Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805/Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler252.flv' => 15,
    'Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805/Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805.3GP' => 15,
     // @codingStandardsIgnoreEnd
    'EM/EM-0028-01.mov' => 30,
    'EchoAlert009/Ea009.m4a' => 0.01,
    'ExcerptFromASacredDuty/asd.avi' => 29.97,
     // @codingStandardsIgnoreStart
    'FreeHdStockFootageTimeLapseOfHighwayTrafficAtDusk/FreeStockFootageTimeLapseOfHighwayTrafficAtDuskInHdFromClipcanvas.mov' => 25,
     // @codingStandardsIgnoreEnd
    'FreedomProjectMovieAndTvSeries-MissionToMir/Mission_to_mir.mov' => 14.97,
    'GreatCitiesDialogue/GreatCities_1.mp4' => 29.97,
    'GreekTelevisionTest/greece-test.mpg' => 25,
    'GrowingYounger/JonAizen-GrowingYounger.mp3' => '',
    'HAMZA-813/87.rmvb' => '',
    'HappyBirthday/HappyBirth.mpg' => 30,
    'HoChak/8tvHoChat.mov' => 15,
    'Holymatrimony/holymatrimony.wmv' => 23,
    'IbisBlancs/ibisblancs.wmv' => 30,
    'IntroToSimedit/Screencap.mov' => [20, 10],
    'Jessies_Sunrise/Jessies_Sunrise.mpg' => 29.97,
    'KOFY_20110607_093000_Paid_Programming/KOFY_20110607_093000_Paid_Programming.mpg' => 29.97,
    'KQED_20100820_150000_Curious_George/KQED_20100820_150000_Curious_George.mpg' => 29.97,
    'Letterman/Letterman.mpg' => 29.97,
    'MITOCW1800intros/MIT_OCW_1800_HfHS_intro.mp4' => 29.97,
    'MITOCW1800intros/MIT_OCW_1800_OCW_intro.mp4' => 29.97,
    'MartyOta_ezEpisode2.TheSmokingGun/smokingun.mov' => 14.99,
    'MendocinoMeteorWeekend/MendocinoMeteorWeekend.wmv' => 30,
    'Metropolis_0/Metropolis.mpeg' => 25,
    'Natebarlow-TalesFromBeyondTrailer399/Natebarlow-TalesFromBeyondTrailer399.mov' => 29.97,
    'No_Local_Stops/No_Local_Stops.mov' => 8,
    'OvercomePt.3-OvercomeTheWorldWithLove_787/Episode15-Overcome-Pt.3-overcomeTheWorldWithLove.m4a' => 6,
    'PhotosAndAudioForPrintDisabledLaunch/LogOfACowboy-Title.mp3' => '',
    'Podcast7_492/Podcast7.m4a' => [3/*trusty*/, 6/*quantal*/],
    'Podcast7_492/Podcast7.mp3' => '',
    'PorYPara/GIRIS.mov' => 25,
    'SamKrutzWinterWonderlandFun/WinterWonderlandFun.mov' => 29.97,
    'SamsFrames/sam_0.avi' => 16,
    'SecondLifeIi/Movie.mov' => 30,
    'ShaneBrennanSittingTourBreathe/breathe.m4b' => 0.07,
    'SitaSingstheBlues_Trailer1/SitaTrailer1.2Sorensen.mov' => 23.98,
    'SkylineSunset_vx2k_1/SkylineSunset_vx2k_1.mpg' => 29.97,
    'SummerSFSunset/SummerSFSunset.mpg' => 25,
    'TRL-2009/TRL-2009.mp4' => 29.97,
    'TestDvRip/DV_Test.mov' => 29.97,
    'TestVideoFromSanyoCamcorder/SANY0001.MP4' => 29.97,
    'The_Phantom_Empire_1935/The_Phantom_Empire_1935.mp4' => [24.36, 59.94],
    'TimBrunsdenMothMan/moth_man.m4v' => 15,
    'VideoEditingWithChris-001/VideoEditingWithChris-001.flv' => 30,
    'WUSA_20091225_000000_9News_Now_Tonight/WUSA_20091225_000000_9News_Now_Tonight.mpg' => 29.97,
    'Xacti1010_trim2_test_2010_02_04/Xacti1010_trim2_test_2010_02_04.mp4' => 29.97,
    'abctest/surabhi.mp3' => '',
    'alc2008/stop_motion.mp4' => 29.97,
    'alc2008/alc2008.mp4' => 29.97,
    'alc2008/alc2008_HQ.mp4' => 29.97,
    'amoalaura/amoaLauraMTV-a.amr' => '',
    'amoalaura/amoaLauraMTV-a.mp3' => '',
    'amoalaura/amoaLauraMTV.wmv' => 15,
    'archivewalk/archivewalk.avi' => 20,
    'babyelephant/babyelephant.avi' => 20,
    'bbc200109111654-1736-TEST/V08591-16.mpg' => 29.97,
    'bikeDiablo/bikeDiablo.avi' => 12,
    'bikeDiabloVideo/bikeDiabloVideo.avi' => 30,
    'camels/camels.avi' => 20,
    'canon-hf-10-test-2008_09_23a/canon-hf-10-test-2008_09_23a.mts' => [29.97, 30],
    'coffeehouse-Freedom-Dance/Freedom-Dance.mov' => 14.99,
    'commute/commute.avi' => 10,
    'demoreel/rj.mpg' => 29.97,
    'dialogcomm/rj.mpg' => 29.97,
    'dick_wakes_up/dick_wakes_up.m4v' => 29.97,
    'dmbb41313/dmbb41313.mp4' => 29.97,
    'dom-25917-watchthisshow-digitalspin1996/Watch_This_Show!_Digital_Spin_1996.pegssc.mpeg' => 29.97,
    'dom-properinterviewsetup/5_20_10final.pegssc.mpeg' => 29.97,
    'drake_saga1/drake_saga1.avi' => 30,
    'drake_saga1_shots/MVI_3983.AVI' => 30,
    'drake_saga1_shots/MVI_3984.AVI' => 30,
    'drake_saga1_shots/MVI_3985.AVI' => 30,
    'drake_saga1_shots/MVI_3986.AVI' => 30,
    'fuss/RJdemov7_1.mpeg4' => 29.97,
    'gov.ntis.testfile/testfile02.mpeg' => 29.97,
    'helios-sunset/helios-sunset.avi' => 7,
    'honeymonth/honeymonth.rm' => 15,
    'hunterbart/hunterbart.mpeg' => 29.97,
    'inauguration2009/inauguration2009_HQ.mp4' => 29.97,
    'inauguration2009/inauguration2009.mp4' => 29.97,
    'k2000/k2000.mov' => 15,
    'kalow_cable_car/kalow_cable_car.mpeg' => 29.97,
    'lotus-drifting/lotus-drifting.HD.mp4' => 29.97,
    'meteorocino/meteorocino.mp4' => 10,
    'morebooks/morebooks.mov' => 29.97,
    'mwaheb-7/na7n-alafdl.rmvb' => 8,
    'mwaheb-7/osama.rmvb' => 8,
    'nightbay/nightbay.aac' => '',
    'nightbay/nightbay.avi' => 30,
    'nightbay/nightbay.mp3' => '',
    'nightbay/nightbay.mp4' => 30,
    'nightbay/MyFatherDidntKnowJohnAdams.mp3' => '',
    'nightbay/nightbay_iphone.mp4' => 30,
    'oldpresidio/oldpresidio.mpeg' => 29.97,
    'pele-frieda/PeleDelappe.avi' => 29.97,
    'promo20061115/promo20061115_350kb.flv' => 15,
    'prores/ProResTestForTracey.mov' => 29.97,
    'prores/422LT_trim.mov' => 25,
    'prueba-02/rebus.avi' => 30,
    'prueba-02/Canto_y_flauta.AVI' => 14.95,
    'rj-jackass/rj-jackass.mpg' => 29.97,
    'rjdemo/rjdemo.mpg' => 29.97,
    'rvb_e37/RvB_Episode37_LoRes.wmv' => 29.97,
    'stairs/stairs.avi' => 20,
    'sunsetstrip/sunsetstrip.flv' => 4,
    'test_clip/test_clip.mpeg' => 29.97,
    'to-SF/to-SF.avi' => 4,
    'traceys-first-time-lapse/traceys-first-time-lapse.avi' => 6,
    'warmy-test/warmy-test/camels.avi' => 20,
    'witness_6447_E007499/fillup shots.mov_bitc.mov' => 25,
    'witness_6447_E007499/fillup shots1.mov_bitc.mov' => 25,
    'witness_6447_E007499/fillup shots2.mov_bitc.mov' => 25,
    ] as $fi => $fps) {
      $i = Video::identify($fi);
      error_log("$fi should be:   ".print_r($fps, 1)."   is:   {$i['fps']}");

      assert(in_array($i['fps'], (array)$fps));
    }
    echo " ALL GOOD!\n\n\n";




    // now test some "ffmpeg -identify" kind of very important checks!!
    $lo = new Locator();
    foreach ([
      // @codingStandardsIgnoreStart
      'Rage2012_201402/Rage2012.wmv' =>
      '{"ac":"wmav2","ar":"48000","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":29.97,"height":"1080","length":1127.3,"pix":"yuv420p","vc":"wmv3","vstreams":"0:2","width":"1440"}',

      'pews/pews.mov' => // rotated 90-degree iphone 1080P test
      '{"ac":"aac","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":1,"fps":24,"height":"1920","length":15.05,"pix":"yuv420p","rotation":"90","vc":"h264","vstreams":"0:0","width":"1080"}',

      'BananaRepublicING/04HallwaysTherapyAquarium.mp3' =>
      ['{"ac":"mp3","ar":"44100","astreams":"0:0","audio":"true","audio_nchannels":2,"fps":null,"height":null,"length":727.66,"width":null,"vstreams":""}',//trusty
            '{"ac":"mp3","ar":"44100","astreams":"0:0","audio":"true","audio_nchannels":2,"fps":null,"height":null,"length":727.6,"width":null,"vstreams":""}', //wily
      ],

      'WUSA_20091118_140000_9News_Now_at_9am' =>
      ['{"DAR":"4:3","PAR":"40:33","ac":"eac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"59.92","height":"480","length":1800.77,"pix":"yuv420p","program":1,"vc":"mpeg2video","vstreams":"0:1","width":"528"}',//trusty
            '{"DAR":"4:3","PAR":"40:33","ac":"eac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":3,"fps":"29.97","height":"480","length":1800.78,"pix":"yuv420p","program":1,"vc":"mpeg1video","vstreams":"0:1","width":"528"}',//wily
      ],

      'WUSA_20091225_000000_9News_Now_Tonight' =>
      '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":336.3,"pix":"yuv420p","program":1,"vc":"mpeg2video","vstreams":"0:1","width":"528"}',

      'OvercomePt.3-OvercomeTheWorldWithLove_787/Episode15-Overcome-Pt.3-overcomeTheWorldWithLove.m4a' =>
      '{"DAR":"1:1","PAR":"72:72","ac":"aac","ar":"44100","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":6,"height":"300","length":3490.09,"pix":"yuvj444p","vc":"mjpeg","vstreams":"0:3","width":"300"}',

      'The_Phantom_Empire_1935/The_Phantom_Empire_1935.mp4' =>
      '{"ac":"aac","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"24.36","height":"480","length":4145.28,"pix":"yuv420p","vc":"h264","vstreams":"0:0","width":"640"}',


      'EchoAlert009/Ea009.m4a' =>
      '{"DAR":"1:1","PAR":"1:1","ac":"aac","ar":"44100","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":"0.01","height":"300","length":4909.46,"pix":"yuvj420p","vc":"mjpeg","vstreams":"0:3","width":"300"}',

      'ExcerptFromASacredDuty/asd.avi' =>
      '{"DAR":"4:3","PAR":"1:1","ac":"mp2","ar":"48000","astreams":"0:0","audio":"true","audio_nchannels":2,"fps":29.97,"height":"480","length":696.51,"pix":"yuv420p","program":1,"vc":"mpeg4","vstreams":"0:1","width":"640"}',


      // item deleted, so check in home dir..
      '/home/tracey/public_html/_/BayrampasaVaillantServisi444_1_494VaillantServisi_1000/8.flv' =>
      '{"audio":"false","fps":23.98,"height":"720","length":29.89,"pix":"yuv420p","vc":"vp6f","vstreams":"0:0","width":"1280","astreams":""}',


      'MITOCW1800intros/MIT_OCW_1800_OCW_intro.mp4' =>
      '{"DAR":"4:3","PAR":"8:9","ac":"aac","ar":"48000","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"29.97","height":"480","length":502.18,"pix":"yuv420p","vc":"h264","vstreams":"0:0","width":"720"}',

      'FOXNEWS_20091125_020000_Hannity' =>
      '{"DAR":"4:3","PAR":"10:11","ac":"ac3","ar":"48000","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"29.97","height":"480","length":3600.36,"pix":"yuv420p","program":1,"vc":"mpeg2video","vstreams":"0:2","width":"704"}',

      'commute/commute.avi' =>
      '{"ac":"mp3","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"10","height":"480","length":115.59,"pix":"yuvj420p","vc":"mjpeg","vstreams":"0:0","width":"640"}',

      'WUSA_20110519_223000_CBS_Evening_News_With_Katie_Couric' =>
      '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":1800.4, "pix":"yuv420p","program":1,"vc":"mpeg2video","vstreams":"0:1","width":"528"}',

      'WUSA_20091019_160000_9News_Now_at_Noon' =>
      '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":1800.8, "pix":"yuv420p","program":1,"vc":"mpeg2video","vstreams":"0:1","width":"528"}',

      'Podcast7_492/Podcast7.m4a' =>
      '{"DAR":"1:2","PAR":"72:72","ac":"aac","ar":"32000","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":3,"height":"320","length":3783.07,"pix":"yuvj444p","vc":"mjpeg","vstreams":"0:3","vsync":"1","width":"160"}',
      // @codingStandardsIgnoreEnd
    ] as $idish => $jsons) {
      $jsons = (array)$jsons;

      if (strncmp($idish, '/home/', 6)===0) {
        $id = '';
        $fi = ltrim($idish, '/');
        $r = ['home', '/'];
      } else {
        list($id, $fi) = explode('/', $idish, 2);
        if (!$fi)
          $fi = "$id.mpg"; // TV

        $r = current($lo->locateUDP($id, 1));
      }


      foreach (['/home/tracey/petabox', '/petabox'] as $pethome) {
        error_log("\n================ CHECKING $id/$fi ($pethome) ===============================================");

        $matched = false;
        foreach ($jsons as $json) {
          $ary = json_decode($json, 1);
          foreach ($ary as $k => &$v) {
            if ($v==='')
              unset($ary[$k]);
          }
          unset($v);

          // @codingStandardsIgnoreLine
          $str = Util::cmd("ssh root@{$r[0]} \"export PETABOX_HOME=$pethome FI='{$r[1]}/{$fi}'; php -r 'require @ia; echo json_encode(Video::identify(@getenv(FI),0,2000,TV::is_tv(@getenv(FI))));' \"  |tail -n1", 'PRINT');
          $ffi = json_decode($str, 1);
          ksort($ffi);


          unset($ffi['ffmpeg-version']);
          unset($ffi['bytes']);
          if ($ary == $ffi) {
            $matched = true;
            break;
          }

          // try rounding duration to nearest *tenth* (not *hundredth*) of a second and retry...
          $ffi['length'] = round($ffi['length'], 1);
          $ary['length'] = round($ary['length'], 1);
          if ($ary == $ffi) {
            $matched = true;
            break;
          }


          error_log('NOT EQUAL!');
          error_log(json_encode($ffi));
          file_put_contents('/tmp/video-regress-shouldBe', print_r($ary, 1));
          file_put_contents('/tmp/video-regress-actually', print_r($ffi, 1));
          $cmd = 'colordiff -U30 /tmp/video-regress-shouldBe  /tmp/video-regress-actually';
          error_log($cmd);
          passthru($cmd);
        }//endforeach

        if (!$matched)
          assert(false);
      }
    }
    error_log("\n\n\nSUCCESS!\n\n\n");
  }
}
