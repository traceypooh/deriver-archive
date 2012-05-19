<?

class Video
{
  public static $PLAYABLES=null;
  const AV_ANTI_DRIFT = ' -muxdelay 5 -copyts -async 88200 -adrift_threshold 5 -dts_delta_threshold 5 ';
  



  // returns the number of seconds, may be fractional
  public static function runtime(Item &$item, $nGroupsAV, $videofile)
  {
    $runtime = ($nGroupsAV<=1 ? $item->getMetaField('runtime') : null);

    if ($runtime)
    {
      if (DEBUG)
        msg($runtime);
      
      if (preg_match('/^[\d\.]+$/', $runtime)) // eg: "60.1"
      {
        $runtime = $runtime;
      }
      else if (preg_match('/^(\d+):(\d+):([\d+\.]+)$/', $runtime, $m))//eg: "01:13:01"
      {
        $runtime = $m[1]*3600 + $m[2]*60 + $m[3];
      }
      else if (preg_match('/^(\d+)\s*min ([\d+\.]+)\s*sec$/', $runtime, $m))//eg: "1min 13sec"
      {
        $runtime = $m[1]*60 + $m[2];
      }
      else if (preg_match('/^(\d+):([\d+\.]+)$/', $runtime, $m))//eg: "13:01"
      {
        $runtime = $m[1]*60 + $m[2];
      }
      else if (preg_match('/^(\d+)\s*minutes (\d+)\s*seconds/', $runtime, $m))//eg: "13 minutes 23 seconds"
      {
        $runtime = $m[1]*60 + $m[2];
      }
      else if (preg_match('/^(\d+)\s*minutes/', $runtime, $m))//eg: "13 minutes"
      {
        $runtime = $m[1]*60;
      }
      else if (preg_match('/^(\d+)\s*seconds/', $runtime, $m))//eg: "10 seconds"
      {
        $runtime = $m[1];
      }
      else
      {
        $runtime = null;
      }
    }

    if (!$runtime)
    {
      return null; //xxxxxxxxxxxxxxxxxxxxxxxxxx too expensive!!
      
      $videofile = ('http://'.$item->getServer().dirname($item->getMainDir()) .
                    "/{$item->identifier}/$videofile");
      if (DEBUG)
        msg("have to fetch identify on $videofile...");

      if ($xxx = Video::identify($videofile))
      {
        $runtime = $xxx['length'];
        if (DEBUG)
          msg($runtime);
      }
    }
    
    return $runtime;
  }
  

  public static function filesToGroupsAV(Item &$item, &$files, $prefix='',
                                         $firstOnly=false,
                                         $onlyThisGroup=null)
  {
    $AUDIO = array('vbr mp3',
                   'ogg vorbis',
                   '64kbps mp3',
                   'mp3 sample',
      );
    
    if (!Video::$PLAYABLES)
    {
      Video::$PLAYABLES = array_flip(
        array(
          'thumbnail2',//keeps all "rank" values !0, also used for 2nd thumbnail..
          'thumbnail', //holder for 1st thumbnail
      
          // our current IA video derivatives:
          'h.264',
          'webm',
          'h.264 720p',
          
          // our prior IA video derivatives:
          '512kb mpeg4',
          'ogg video',
          
          // formats users might upload that we can play:
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
          '64kbps mp3',
          'mp3 sample',
          ));
    }

    $HIRANK = 100;
    

    // cluster original files with their derivatives
    $groups= array();
    $audio = array();
    $srts  = array();
    $durations = array();
    $lengths = array(); // used to "prefer" the <length> in deriv <file>

    $t2 = Video::$PLAYABLES['thumbnail2'];
    
    $groupsA = array();
    $groupsV = array();
    foreach ($files as $filocation => $fi)
    {
      $short = ltrim($filocation, '/');
      $formatLC = strtolower($fi['FORMAT']);
      $suffixLC = strtolower(strrchr($short, '.'));

      if ($suffixLC == '.srt')
      {
        $srts[] = $short; // save for later
        continue;
      }
      
      $rank = Video::$PLAYABLES[$formatLC];
      if ($suffixLC=='.mp3'  &&  !$rank)
      {
        // OK some items get super specific on the bitrates, like "32Kbps MP3" eg:
        //    /details/Dragnet_OTR
        // so for them, let's pretend they are like less-preferred MP3 <format>
        // so they'll actually show up in the A/V player, etc....
        $formatLC = '64kbps mp3';
        $rank = Video::$PLAYABLES[$formatLC];
      }
          
      if (isset($fi['ORIGINAL']))
      {
        // derivative!
        $deriv = true;
        $key = $fi['ORIGINAL'];
      }
      else
      {
        // original file!
        $deriv = false;
        $key = $short;
      }

      if ($onlyThisGroup  &&  $key!=$onlyThisGroup)
        continue;

      if (!$deriv)
      {
        // use file's explicit <title> if it exists;
        // else (later, base (no subdirs) filename, w/o final suffix)
        $name = $fi['TITLE'];
        $groups[$key]['TITLE'] = $name;
      }
      else
      {
        // derivative -- use any title it has as backup/2nd choice
        // for example an unlogged in user viewing a Grateful Dead
        // soundboard stream-only item -- they never see the original
        // in the file loop iteration we are in!
        if ($fi['TITLE']  &&  !isset($groups[$key]['TITLE']))
          $groups[$key]['TITLE'] = $fi['TITLE'];
      }

      if (isset($fi['LENGTH']))
      {
        // convert to number of seconds
        $sec = $fi['LENGTH'];
        if (preg_match('/^(\d+):(\d+)$/', $sec, $mat))
          $sec = (60*$mat[1])+($mat[2]);
        else if (preg_match('/^(\d+):(\d+):(\d+)$/', $sec, $mat))
          $sec = (3600*$mat[1])+(60*$mat[2])+($mat[3]);

        $durations[$short] = $sec;
        
        // shn file durations seem wacky, eg:
        //   /details/bdolzani2011-06-28
        // so we'll "prefer" derivs' LENGTH when we can...
        if ($deriv)
          $lengths[$key]=1; // set to indicate derivative set the LENGTH

        if ($deriv  ||  !isset($groups[$key]['LENGTH'])  ||
            !isset($lengths[$key]))
        {
          $groups[$key]['LENGTH'] = round($sec);
        }
      }

      
      if (!$rank)
      {
        // not a playable file, skip it -- now that we've looked for
        // its TITLE and/or saved it ;-)
        continue; 
      }


      if ($deriv)
      {
        if ($formatLC=='h.264 720p')
        {
          $groups[$key]['HD'] = $short;
          continue;
        }
      }
      else
      {
        // tracey = lame. Court submits exhibit #2 on behalf of The People vs..
        // We **LOVE** this item/movie!
        //    http://archive.org/details/Sita_Sings_the_Blues
        // but it has a large number of h.264 video <format> files in it
        // and we needed someway to filter out monstrously large "HD" ones...
        if ($formatLC=='h.264 hd'  ||
            // eg: archive.org/details/bancroftlibraryucberkeley
            ($formatLC=='h.264 mpeg4'  &&  (strpos($name,'[HD]')  ||
                                            strpos($name,' HD ')  ||
                                            preg_match('/ HD$/',$name))))
        {
          $groups[$key]['HD'] = $short;
          continue;//chexxx
        }
        
        if ($rank > 1)
          $rank += $HIRANK; // deprioritize non-derivatives!
      }
        

      if ($rank > 1)
      {
        // ie: not thumbnail
        if (in_array($formatLC, $AUDIO))
        {
          $groupsA[$key] = 1;
          $audio[$short] = 1;
        }
        else
        {
          $groupsV[$key] = 1;
        }
      }
          
      if (!isset($groups[$key][$rank]))
      {
        $groups[$key][$rank] = $short;
        /**/ if ($rank < $HIRANK  &&  isset($groups[$key][$rank + $HIRANK]))
          unset($groups[$key][$rank + $HIRANK]);
        else if ($rank > $HIRANK  &&  isset($groups[$key][$rank - $HIRANK]))
          unset($groups[$key][$rank - $HIRANK]);
      }
      else if ($formatLC=='thumbnail'  &&  !isset($groups[$key][$t2]))
      {
        $groups[$key][$t2] = $short;
      }
    }
    ksort($groups);



    $nAudio = count($groupsA);
    $nVideo = count($groupsV);
    $showing = (($nVideo > 0  &&
                 ($nVideo >= $nAudio ||
                  $item->getMetaField('mediatype')=='movies')) ? 'movies' :
                ($nAudio ? 'audio' : false));
    if (DEBUG)
    {
      msg("DURATIONS");
      printr($durations);
      msg("PLAYABLES");
      printr(Video::$PLAYABLES);
      msg("GROUPS");
      printr($groups);
      msg("groupsAV (a:".count($groupsA).",v:".count($groupsV).") audio:");
      printr($audio);
    }
    $groupsAV = ($showing == 'movies' ? $groupsV : $groupsA);

    
    


    $grouped = array();
    
    foreach ($groups as $orig => $rankToFile)
    {
      if (!isset($groupsAV[$orig]))
        continue;
      
      $kv = array();
      
      ksort($rankToFile);
      if ($rankToFile[0])
        $kv['POSTER'] = $prefix.$rankToFile[0];
      else if ($rankToFile[1])
        $kv['POSTER'] = $prefix.$rankToFile[1];


      
      foreach ($rankToFile as $rank => $src)
      {
        if (is_numeric($rank)  &&  $rank>1) // not a POSTER or TITLE!
        {
          if (($showing=='movies'  &&   $audio[$src])  ||
              ($showing!='movies'  &&  !$audio[$src]))
          {
            continue;
          }
          
          $kv['SRC'][] = $prefix.$src;
        }
      }

      if (!isset($kv['SRC']))
        continue; // no derivatives in video (or audio) group of files, likely!
      

      
      $name = $rankToFile['TITLE'];
      if (!$name)
      {
        $name = str_replace(
          '_',' ', preg_replace('/_512kb$/','',
                                preg_replace('/\.[^\.]+$/','',
                                             basename($orig)))); //nix suffix
      }
      $kv['TITLE'] = $name;
      $kv['ORIG']  = $orig;
      if (isset($rankToFile['LENGTH']))
      {
        $kv['LENGTH'] = $rankToFile['LENGTH'];
      }
      else
      {
        // this *could* get a bit expensive esp. for items w/ tons of videos!!
        // xxxx should cache this & entire HTML from /details/ page for X mins...
        $kv['LENGTH'] = Video::runtime($item, count($groupsAV), $orig);
      }
      if (isset($rankToFile['HD']))
        $kv['HD'] = $rankToFile['HD'];

      $grouped[] = $kv;
    }

    if (DEBUG)
    {
      msg("filtered final set:");
      printr($grouped);
    }



    $captions = ($showing=='movies' ?
                 Video::captions($grouped, $srts, $item->identifier) : array());
    
    return array(($firstOnly ? $grouped[0] : $grouped),
                 count($groupsAV),
                 $showing,
                 $captions,
                 $durations);
  }


  // setup subtitles/captions
  private static function captions(&$groups, &$srts, $identifier)
  {
    if (!count($srts))
      return array();

    if (DEBUG)
      printr($srts);
    
    
    $captions = array();
    $captioned = false;
    
    foreach ($srts as $srt)
    {
      // given semi-arbitrary names like:
      //   "movie.srt"
      //   "track1.srt"
      //   "track1.fr.srt"
      //   "track1.sp.srt"
      // try to match it to the most appropriate video clip
      $base1 = substr($srt, 0, strlen($srt)-4); // nix ".srt" suffix
      $base2 = preg_replace('/[_\-\.][^_\-\.]+$/','', $base1);

      $lang='';      
      if ($base2 != $base1)
      {
        // save the likely "language" portion we just found
        $lang = strtolower(substr($base1, strlen($base2)+1));
        // srt:   track1.something.srt
        // base1: track1.something
        // base2: track1
        // lang:  something
      }
      //msg("$srt --- $base1 --- $base2 --- $lang");
    
      // now search the videos to see what it matches best to (if any)
      $bestmatch1='';
      $bestmatch2='';

      for ($idx=0; $idx < count($groups); $idx++)
      {
        $clip = $groups[$idx]['ORIG'];

        // remove any suffix
        $vidname = preg_replace('/(_512kb)*\.[^\.]+/', '', $clip);
        //printr("($idx) [$vidname] vs:");
        //printr(array($base1,$base2));
        
        if ($vidname == $base1)  $bestmatch1 = $idx;
        if ($vidname == $base2)  $bestmatch2 = $idx;
      }
      
      if  ($bestmatch1 !== '') $vidN = $bestmatch1;//NOTE: prefer longest match!
      else if ($bestmatch2 !== '') $vidN = $bestmatch2;
      else if (count($groups)==1) $vidN = 0;//only *1* video track -- match!
      else continue;

      if (DEBUG)
        printr("bestmatch1:$bestmatch1 --- bestmatch2:$bestmatch2");
      
      if (!isset($captions[$vidN]))
        $captions[$vidN] = array();
      
    
      if (!$lang)
      {
        // default to "en" if no language found above and it hasnt been "used" yet
        if (!isset($captions[$vidN]['en']))
        {
          $lang = 'en';
        }
        else
        {
          // basically back off / give up and use the filename
          $lang = basename($srt);
          $lang = substr($lang, 0, strlen($lang)-4); // nix ".srt" suffix
        }
      }
    
    
    
      $captions[$vidN][$lang] = "/download/$identifier/$srt";

      //printr("BESTMATCH: $bestmatch1 $bestmatch2 $vidN $lang");
      //printr($captions);

      $captioned = true;
    }
    //printr($captions);

    if (!$captioned)
      return array();

    return $captions;
  }
  

  // returns basic A/V info for given file in array, using "ffmpeg -i".
  //
  // runs a ffmpeg cmd to identify the video/audio parameters of a file
  // and returns an array of key/val params, eg:
  // (
  //  [width]   => 640
  //  [height]  => 480
  //  [fps]     => 20      (may be fractional, may be like "48k")
  //  [length]  => 2.75
  //  [audio]   => true
  //  [ar]      => 44100
  //  [ac]      => aac
  //  [vc]      => h264
  // )
  public static function identify($file, $print=false, $head=150, $skip_rotation_check=false)
  {
    $ffmpeg = configGetPetaboxPath('bin-ffmpeg');

    // kill if hangs after 30 sec!
    // we give it this long since some crazy *NON* interleaved AVI files
    // can take nearly 20+ seconds to inspect...
    
    // NOTE: we ignore the exit status of this command because it *will*
    // fail with a "Must supply at least one output file" message.
    // Give it (only) a short time to run (everyonce in awhile it repeat logs
    // an error message, which runs deriver proc out of memory)
    // only keep the first 150 lines (in case it went crazy logging)
    $lines = array();
    $retval = null;
    exec(Util::timeout(600)." $ffmpeg -an -vn -i ".Util::esc($file)." 2>&1 | ".Util::timeout(600)." head -$head",
         $lines, $retval);
    
    if ($print)
    {
      echo "  ##################  CLIP INFO  ################ \n  # ".
        implode($lines, "\n  # ").
        "\n  ##################  CLIP INFO  ################ \n";
    }
    
    $out = implode($lines, "\n");



    $video='';
    if (preg_match('/\n *(Stream .*? Video: .*?, (\d+)x(\d+)(, | \[)(.*))/', $out,
                   $mat))
    {
      $width = $mat[2];
      $height= $mat[3];
      // save this for later -- we gonna parse other params from it
      // wacky items can have 2+ video streams in them!  so we pick the
      // first one that has parseable width/height
      // eg: /details/FOXNEWS_20091125_020000_Hannity
      // has 2 Video streams, and the 1st one is junky, 2nd one we want!
      $video = $mat[1];
    }
    else
    {
      $width = null;
      $height= null;
    }
      
    
    // compute length of movie in seconds (may have decimal component)
    // compute frames per second of movie (may have decimal component)
    $arr = array(
      'width'   => $width,
      'height'  => $height,
      'fps'     => null,
      'audio'   => 'false',
      
      'length' => (preg_match('/\n *Duration: (\d+):(\d+):([\d\.]+)/', $out,
                              $mat) ? ((60 * 60 * $mat[1]) +
                                       (60 * $mat[2]) +
                                       $mat[3]) : null),

      'ffmpeg-version' =>(preg_match('/^FFmpeg version SVN-r(\d+)/', $out, $mat) ?
                          $mat[1] :
                          (preg_match('/^(FF|ff)mpeg version (git-[^,]+)/',$out,$mat) ?
                           $mat[2] : '')),
      );

    /*if (($sec = $arr['length']) > 86400)//hold off for now...
    {
      echo "Duration of $sec seconds seems wrong...";
      $mylen = TV::alt_duration($file);
      if ($mylen < $sec  &&  $mylen > 10)
      {
        // Seems like we have a more accurate length.
        echo "Using alternate method!\n";
        $arr['length'] = $mylen;
      }
    }*/
    

    // For TV archive items, we want to always use the first audio track found
    // Save 2 streams like: "0.1,0.2"
    if (preg_match_all('/ Stream \#(\d+.\d+).*?(: Audio: .*)/', $out, $alines))
    {
      //printr($alines);
      $arr['astreams'] = '';
      $arr['audio_nchannels'] = 0;
      $best = array(); // prefer mono and stereo
      $rest = array(); // .. and o/w take any riff-raff if need be...
      for ($lineN=0; $lineN < count($alines[0]); $lineN++)
      {
        $aline    = $alines[0][$lineN];
        $streamID = $alines[1][$lineN];

        // oneiric era + goes from/to:
        // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
        // Audio: mp1 ([6][0][0][0] / 0x0006), 48000 Hz, stereo, s16, 416 kb/s
        $aline = preg_replace('= \(\[\d+\]\[\d+\]\[\d+\]\[\d+\] / 0x\d+\)=','',
                              $aline);
        
        if (count($alines[0])==3  &&
            // throw out suspected-decoded-wrong audio streams like:
            // Audio: mp1, 48000 Hz, stereo, s16, 416 kb/s
            // /details/WUSA_20110519_223000_CBS_Evening_News_With_Katie_Couric
            (strpos($aline, ': Audio: mp1,')>0  ||
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091019_160000_9News_Now_at_Noon
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)')>0  || //mar2012 version
             strpos($aline, ': Audio: aac, 24000 Hz, 7.1(wide)')>0  || //nov2011 version
             // throw out suspected-decoded-wrong audio streams like:
             // /details/WUSA_20091118_140000_9News_Now_at_9am    -- it should be noted this item is botched/glitchy
             strpos($aline, ': Audio: aac, 24000 Hz, 8 channels (FL+FR+FC+LFE+BL+BR+FLC+FRC)')>0  || //mar2012 version
             strpos($aline, ': Audio: aac, 7350 Hz, 3 channels')>0 )) //nov2011 version
        {
          continue;
        }
        
        if (preg_match('/, (\d+) channels, /', $aline, $m))
        {
          if ($m[1]==0)
          {
            // throw out audio tracks w/ 0-channels
            // eg on natty, /details/WUSA_20091225_000000_9News_Now_Tonight
            continue;
          }
          else if ($m[1] <= 2)
          {
            $best[] = $streamID; // mono/stereo track.  we likey!
          }
          else
          {
            $rest[] = $streamID; // surround-ish track.  2nd choice..
          }

          $arr['audio_nchannels'] += $m[1];
        }
        else if (preg_match('/, stereo, /', $aline, $m))
        {
          $arr['audio_nchannels'] += 2;
          $best[] = $streamID;
        }
        else if (preg_match('/, mono, /',   $aline, $m))
        {
          $arr['audio_nchannels'] += 1;
          $best[] = $streamID;
        }
        else
        {
          $rest[] = $streamID;
        }
        

        $arr['audio']='true';
        
        if (!isset($arr['ac'])  &&  preg_match('/: Audio: ([^, ]+)/', $aline,$mm))
          $arr['ac'] = $mm[1];

        // Some source MP3s make ffmpeg fail to output a WAVE unless we parse and
        // pass on the audio sample rate, eg: /details/BananaRepublicING
        if (!isset($arr['ar'])  &&  preg_match('/: Audio: .*?, ([\d\.]+) Hz/',
                                               $aline, $mm))
        {
          $arr['ar'] = $mm[1];
        }
      }
 
      // basically make comma-separated string of: $best,$rest
      $best = array_flip($best);
      $rest = array_flip($rest);
      $arr['astreams'] = join(',', array_keys($best + $rest));
    }
    

    
    // Set the framerate
    if (preg_match('/Stream .*? Video: (.*)/', $video, $m))
    {
      $tmp = Video::fps($file, $m[1]);
      if (is_numeric($tmp))
      {
        if ($tmp > 0)
          $arr['fps'] = $tmp;
      }
      else if ($tmp !== "")
      {
        $fps = Video::tofloat($tmp);
        // true story for real! /details/OvercomePt.3-OvercomeTheWorldWithLove_787
        // has frame rate of "0"
        if ($fps != 0)
          $arr['fps'] = $fps;
      }
    }
    
    
    

    // save aspect-related information
    if (preg_match('/Stream .*? Video: .*?(\[|, )[SP]AR (\d+:\d+)/', $video, $m))
      $arr['PAR'] = $m[2];
    if (preg_match('/Stream .*? Video: .*?(\[|, )[SP]AR (\d+:\d+) DAR (\d+:\d+)/', $video,$m))
      $arr['DAR'] = $m[3];

    if (preg_match('/Stream \#(\d+.\d+).*?: Video: /', $video, $m))
      $arr['vstreams'] = $m[1];
    

    // save the Audio and Video Codecs and other info used for video..
    if (preg_match('/: Video: ([^, ]+)/', $video, $m))       $arr['vc' ] = $m[1];

    if (preg_match('/: Video: [^,]+, ([^,]+),/', $video, $m))$arr['pix'] = $m[1];


    // now see if there is a rotation exif tag, like some iphone4+ videos
    if (!$skip_rotation_check)
    {
      $exiftool = configGetPetaboxPath('bin-exiftool');
      // suppress any warnings if in !print mode
      $cmd = Util::timeout(7200)." $exiftool -Rotation ".Util::esc($file).($print?"":" 2>/dev/null")." | cut -f2 -d:";
      $rot = trim(Util::cmd($cmd, 'CONTINUE', ($print ? 'PRINT' : 'QUIET')));
      if (is_numeric($rot)  &&  $rot!=="0")
      {
        $arr['rotation'] = $rot;
        if ($arr['rotation'] == 90)
        {
          // swap width and height
          $tmp = $arr['width'];
          $arr['width'] = $arr['height'];
          $arr['height'] = $tmp;
        }
      }
    }
    
    return $arr;
  }


  public static function identify2($file, $dvddevice='')
  {
    $mplayer = configGetValue('bin-mplayer');

    $lines = array();

    // kill if hangs after 30 sec!
    // we give it this long since some crazy *NON* interleaved AVI files
    // can take nearly 20+ seconds to inspect...
    
    // NOTE: we ignore the exit status of this command because it *will*
    // fail with a "Must supply at least one output file" message.
    // Give it (only) a short time to run (everyonce in awhile it repeat logs
    // an error message, which runs deriver proc out of memory)
    // only keep the first 250 lines (in case it went crazy logging)
    $retval = null;
    
    $info = exec(Util::timeout(600)." $mplayer -identify -nosound -novideo -nolirc -frames 0 ".Util::esc($file).($dvddevice ? ' -dvd-device '.Util::esc($dvddevice) : '')." 2>/dev/null | ".Util::timeout(600)." head -250",
                 $lines, $retval);
    return implode($lines, "\n");
  }


  // given a duration (in seconds), return something like "HH:MM:SS", eg:
  //  01:12:27    (for 1 hr, 12 minutes, 27 seconds)
  public static function hms($seconds, $omit00hr=false)
  {
    $hr = floor($seconds/3600);
    $min= floor(($seconds - ($hr*3600))/60);
    $sec= $seconds - ($hr*3600) - ($min*60);
    if ($omit00hr  &&  $hr==0)
      return sprintf("%02d:%02d", $min, $sec);
    return sprintf("%02d:%02d:%02d", $hr, $min, $sec);
  }

  
  public static function hms2sec($hms)
  {
    if (preg_match('/^(\d+):(\d+):([\d+\.]+)$/', $hms, $m)) //eg: "01:13:01"
      return $m[1]*3600 + $m[2]*60 + $m[3];
    else
      return false;
  }
  


  private static function tofloat($rate)
  {
    if (intval($rate)==0)
      return 0;
    if (preg_match('=^([\d\.]+)/([\d\.]+)$=', $rate, $m))
      return round($m[1] / $m[2],2);
    return 0;
  }
  

  // uses "exiftool" to identify characteristics of file.
  // returns: php hashmap from json output of tool; fales on errors.
  public static function EXIF($file, $quiet=false)
  {
    $exiftool = configGetPetaboxPath('bin-exiftool');
    list($retval,$js)=Util::cmd(Util::timeout(7200)."$exiftool -json ".Util::esc($file).($quiet?" 2>/dev/null":""),'CONTINUE','EXIT_STATUS');
    if ($retval!=0)
      return false;

    $ary = json_decode($js,true);
    if (count($ary))
      return $ary[0];

    return false;
  }


  // tries to return fps of file, using "exiftool".
  // returns false on errors.
  public static function fpsEXIF($file)
  {
    if (!($exif = Video::EXIF($file,true)))
      return false;
    
    if (isset($exif['VideoFrameRate']))
      return rtrim($exif['VideoFrameRate']," fps");

    return false;
  }

  // uses "ffprobe" and returns string like "30000/1001" (for ~29.97)
  // OR sometimes a float, depending!
  public static function fps($src, $vline='')
  {
    // /details/MITOCW1800intros
    // has 2 videos which confused ffprobe mar2012, but this fps line
    // *is* present in the Video: line.  if any video is basically claiming
    // that in the identify line, we go with that!
    if (preg_match('/, 29.97 fps,/', $vline))
      return 29.97;

    
    $ffprobe = configGetPetaboxPath('bin-ffprobe');

    $streams = Util::cmd("$ffprobe -show_streams ".Util::esc($src)." 2>/dev/null  | tr '\n' ';' | perl -pe 's=\[/STREAM\]=\n=g; s=\[STREAM\]=\n\[STREAM\]=g'|fgrep '[STREAM]'|cat", 'ARRAY');

    $astreams = $vstreams = $goods = array();
    foreach ($streams as $stream)
    {
      $kvs = explode(';', $stream);
      $ary = array();
      foreach ($kvs as $kv)
      {
        if (strpos($kv,'=')===FALSE)
          continue;
        list($key,$val) = explode('=', $kv);
        $ary[$key]=$val;
      }

      if ($ary['codec_type']=='video')
      {
        // If not really parseable stream, we'll effectively note that for later..
        if ($ary['width']!=='0')
          $goods[] = $ary; 

        $vstreams[] = $ary;
      }
      
      if ($ary['codec_type']=='audio')
        $astreams[] = $ary;
    }
    //printr($vstreams);
    //printr($astreams);


    if (count($vstreams) > 1  &&  count($goods) > 0  &&
        count($vstreams) > count($goods))
    {
      // throw out any video stream whose dimensions cant be parsed!
      // eg: /details/FOXNEWS_20091125_020000_Hannity
      $vstreams = $goods;
    }
    

    $ptr = (count($vstreams) ? $vstreams : $astreams);


    
    // "r" vs "avg" discrepancy items, eg:
    //    /details/The_Phantom_Empire_1935
    if (count($ptr) == 1  &&
        isset($ptr[0]['r_frame_rate'])  &&
        isset($ptr[0]['avg_frame_rate']))
    {
      $r   = Video::tofloat($ptr[0]['r_frame_rate']);
      $avg = Video::tofloat($ptr[0]['avg_frame_rate']);

      if ($r < 1  &&  $avg < 1  &&  ($ret=Video::fpsEXIF($src)))
        return $ret;
      
      if ($r < 1    &&  $avg > 0  &&  $avg < 100)
        return $ptr[0]['avg_frame_rate'];

      if ($r > 100  &&  $avg > 0  &&  $avg < 100)
        return $ptr[0]['avg_frame_rate'];
    }
    


    $rates = array();
    foreach ($ptr as $stream)
    {
      foreach ($stream as $key => $val)
      {
        if (preg_match('/frame_rate$/', $key))
        {
          $rate = $val;
          
          if (intval($rate)!=0)
          {
            // take the 1st one found (almost always "r_frame_rate")


            // check for wack-ness
            // Eg: quite a few ".m4a" files which seem to be mostly m4a audio
            // but have a video motion-JPEG frame very very infrequently.
            // anyway, they aren't no 600 fps!!
            if ($rate=='600/1'  ||  // /details/Podcast7_492
                $rate=='600/2094059')// /details/OvercomePt.3-OvercomeTheWorldWithLove_787
            {
              $rate = '6/1'; 
            }
            else if ($rate == '60000/1001'  &&
                     $stream['codec_time_base']=='1/2997')
            {
              $rate = '30000/1001'; // eg: /details/ExcerptFromASacredDuty
            }

            $rates[] = $rate;
          }
        }
      }
    }


    if (!count($rates))
      return "";

    // pick first rate
    return $rates[0];
  }


  // lte: "less than or equal?" -- as in "return keyframe that has time that
  //      is "less than or equal" to the "time" param
  public static function idxTimeToKeyframeByte($src, $time=null, $lte=true, $byteIsEndOfKeyframe=false)
  {
    if (!file_exists("$src.idx"))
      fatal("file does not exist {$src}.idx");

    $fh = null;
    try
    {
      // construct the map(ping) of [second position] => [start byte] 
      // for keyframes the .idx file
      $cmd = "egrep '^# Timestamp|^V |^Picture ' ".Util::esc("$src.idx")."|egrep -A1 '^V |^Picture ' 2>/dev/null";
      $fh = popen($cmd, "r");
      $firstTS = -1;
      $priorStartByte = 0;
      $priorEndByte = 0;
      $fps = 29.97;
      $map = array();

      while (!feof($fh))
      {
        $line = trim(fgets($fh));
        if ($line=="")
          continue;
        //error_log($line);
        
        $tok = preg_split('/[\s]+/', $line);
        
        if ($tok[0]=='V'  &&  is_numeric($tok[2]))
        {
          $framen = intval($tok[2]);
          list($startByte,$thisIs0,$nBytes) = explode(',', $tok[4]);
          $nBytes    = hexdec($nBytes);
          $startByte = hexdec(ltrim($startByte,'I:'));
          $endByte   = $startByte + $nBytes;
          //error_log("$startByte + $nBytes => $endByte");
        }
        else if ($tok[0]=='#'  &&  $tok[1]=='Timestamp')
        {
          $ts = $tok[2];
          if (!preg_match('/^(\d+):(\d+):(\d+),(\d+)$/', $ts, $mat))
            fatal("bad time in: $line");
          
          $ts = (3600*$mat[1])+(60*$mat[2])+($mat[3])+($mat[4]/$fps);
          if ($firstTS == -1)
            $firstTS = $ts;
          $ts = round($ts - $firstTS,2);

          if ($time===null)
          {
            $map["$ts"] = $startByte;
          }
          else if ($lte   &&  $ts >= $time)
          {
            pclose($fh);
            return ($byteIsEndOfKeyframe ? $endByte : $priorStartByte);
          }
          else if (!$lte  &&  $ts >= $time)
          {
            pclose($fh);
            return ($byteIsEndOfKeyframe ? $endByte : $startByte);
          }
          

          $priorStartByte = $startByte;
          $priorEndByte   = $endByte;
        }
        else if ($tok[0]=='Picture'  &&  $tok[6]=='fps'  &&  is_numeric($tok[5]))
        {
          $fps = floatval($tok[5]) / 1000;
        }
        else
        {
          fatal("unexpected format line from avidemux idx: $line");
        }
      }
    }
    catch (Exception $e)
    {
      if ($fh)
        pclose($fh);
      throw $e;
    }
    if ($fh)
      pclose($fh);

    if ($time===null)
      return $map;
    else
      fatal("$time not found!");
  }

  

  // NOTE: this should prolly entirely replace the normal method since
  //       tough shows like /details/CNNW_20120311_080000_CNN_Newsroom
  //       now work again with it...
  private static $frame2sec        = array();
  private static $sec2keyframeByte = array();
  private static $frame2secLastSrc = null;
  public static function framenum2secALT($src, $framenum)
  {
    if (Video::$frame2secLastSrc != $src)
    {
      Video::$frame2secLastSrc = $src;
      
      $ffmpeg = configGetPetaboxPath('bin-ffmpeg');

      Video::$frame2sec = array();
      Video::$sec2keyframeByte = array();
      
      // Example output:
      // [showinfo @ 0x2f469a0] n:0 pts:1992578 pts_time:1.99258 pos:264892 fmt:yuv420p sar:10/11 s:704x480 i:T iskey:1 type:I checksum:0541DE56 plane_checksum:[13E0F3DC 9D24758E 5DF874DD 00000000]
      $fh=popen("$ffmpeg -i ".Util::esc($src)." -vf showinfo=n -f null . 2>&1 |fgrep showinfo", "r");
      try
      {
        while (($line = fgets($fh)))
        {
          if (preg_match('|\] n:(\d+) pts:(\d+) pts_time:([\d\.]+) pos:(\d+) .*? iskey:(\d)|', $line, $mat))
          {
            list(,$n,$pts,$pts_time,$pos,$iskey) = $mat;
            Video::$frame2sec[$n] = $pts_time;
            if ($iskey)
              Video::$sec2keyframeByte[$pts_time] = $pos;
          }
        }
      }
      catch (Exception $e)
      {
        fclose($fh);
        throw($e);
      }
      if ($fh)
        fclose($fh);
    }
    
    //printr(Video::$frame2sec);
    //printr(Video::$sec2keyframeByte);

    if (isset(Video::$frame2sec[$framenum]))
      return floor(Video::$frame2sec[$framenum]);
    else
      return -1;  // give up! (caller can fallback)
  }



  // NOTE: $framenum can be an integer (frame#) OR a thumbnail filename created
  // by ffmpeg (with the framenumber encoded in the name)
  private static $lastmap    = array();
  private static $lastmapSrc = null;
  private static $lastmapALT = false;
  public static function framenum2sec($src, $framenum, $identify)
  {
    if (!is_numeric($framenum))
    {
      if (!preg_match('/(\d+).jpg$/', basename($framenum), $mat))
        fatal("bad ffmpeg thumbnail name!");
      $framenum = intval($mat[1]);
    }

    if (($identify['vc']=='mpeg2video'  ||  
         $identify['vc']=='mpeg1video'  ||
         $identify['vc']=='h264')  &&  // for format = "h.264/MPEG2-TS"!
        file_exists("$src.idx"))
    {
      // format we are parsing out will be group of pictures (starts with 'V')
      // followed by a comment that indicates the start time position

      // construct the map(ping) of [framenum] => [timestamp] from the .idx file
      if (Video::$lastmapSrc != $src)
      {
        $lastTS = -666;
        $cmd = "egrep '^# Timestamp 2>/dev/null |^V ' ".Util::esc("$src.idx")."|egrep -A1 '^V ' 2>/dev/null";
        $fh = popen($cmd, "r");
        try
        {
          $firstTS = -1;
          // basically we gonna cache this map in case we asked for *another*
          // mapping after this call is done!
          Video::$lastmap = array();
          Video::$lastmapSrc = $src;
          Video::$lastmapALT = false;
          
          while (!feof($fh))
          {
            $line = trim(fgets($fh));
            if ($line=="")
              continue;
            //error_log($line);
            
            $tok = preg_split('/[\s]+/', $line);
            
            if ($tok[0]=='V'  &&  is_numeric($tok[2]))
            {
              $framen = intval($tok[2]);
            }
            else if ($tok[0]=='#'  &&  $tok[1]=='Timestamp')
            {
              $time = $tok[2];
              if (!preg_match('/^(\d+):(\d+):(\d+),(\d+)$/', $time, $mat))
                fatal("bad time in: $line");
              
              $ts = floor((3600*$mat[1])+(60*$mat[2])+($mat[3])+($mat[4]/30));
              if ($firstTS == -1)
                $firstTS = $ts;
              $ts = $ts - $firstTS;

              // SANITY CHECK!!
              //error_log("$ts < $lastTS");
              if ($ts < $lastTS)
              {
                error_log("UHOH .idx file timestamps are not monotonic!  Fallback to alternate method...");
                Video::$lastmapALT = true;
                break;
              }
              
              $lastTS = $ts;

              Video::$lastmap[$framen] = $ts;
            }
            else
              fatal("unexpected format line from avidemux idx: $line");
          }
          //printr(Video::$lastmap);
          
          // check for avidemux may be all 00:00:00,000!
          // eg: /details/GreekTelevisionTest
          if (count(array_unique(array_values(Video::$lastmap))) == 1)
          {
            error_log("UHOH .idx all timestamps are identical!  Fallback to alternate method...");
            Video::$lastmapALT = true;
          }
        }
        catch (Exception $e)
        {
          if ($fh)
            pclose($fh);
          throw $e;
        }
        if ($fh)
          pclose($fh);
      }
      

      
        
      if (Video::$lastmapALT)
        return Video::framenum2secALT($src, $framenum);

      $best = 0;
      $bestfound = false;
      foreach (Video::$lastmap as $framen => $ts)
      {
        if ($framen >= $framenum)
          $bestfound = true;
        else
          $best = $ts;
      }

      if (!$bestfound)
        return -1;  // give up! (caller can fallback)

      // this is the I-frame that is at or before the passed in frame number!
      // echo "\n\nNEW TS: $best\n";
      return $best;
    }
    else if ($identify['fps'])
    {
      return round($framenum / $identify['fps']);
    }
    return -1; // give up! (caller can fallback)
  }


  public static function yadif_nix($str)
  {
    return
      str_replace('-vf yadif=0:-1:1,','-vf ',
                  str_replace('-vf yadif=1:-1:0 ',' ',
                              str_replace('-vf yadif,','-vf ',
                                          str_replace('-vf yadif ', ' ', $str))));
  }
  

  // runs a ffmpeg cmd to create frames for a film,
  // suitable for thumbnails or an animated GIF
  //
  // returns list of images made on SUCCESS;
  // returns null if maker hangs and timesout
  //
  // NOTE: throws exception on errors
  public static function frames($file, $outDir, $tvarchive=false, $maxThumbs=0,
                                $identify=null, $timestampBaseName=null,
                                $dimens='-s 160x110', $xtraArgs='-b:v 1K')
  {
    $ffmpeg = configGetPetaboxPath('bin-ffmpeg').' -v 0';

    // find information on the film, like
    // length of the movie in seconds (note might have decimal accuracy)
    // frames per second (note might have decimal accuracy)
    if (!$identify)
    {
      $identify = Video::identify($file, true, 150, $tvarchive);
      print_r($identify);
    }

    
    list($vf,$outW,$outH) = Video::vf($dimens, $identify,
                                      $tvarchive,
                                      /*padBlack==*/true);


    
    $length = ($identify['length'] ? $identify['length'] : 0);
    

    // determine the "rate" -- the number of frames/sec to extract.
    // max limit for number of thumbs.  sometimes the deriver misinterprets
    // certain video formats and cannot get the "every 30 seconds" correct
    // and does things like "every 30 frames" or whatnot.
    // max limit used to be 1000, but that's still a lot of frames when there
    // is this bug.
    // so a 2 hour film == 120 minutes == 240 frames (for every 30 seconds...)
    if ($tvarchive)
    {
      $frameLimit = 5000;
      $frameEvery = 30;
    }
    else
    {
      $frameLimit = 250;
      
      $frameEvery = 30; // default is frame every 30 seconds
      if ($length == 0)
        $frameLimit = 30;
      else if ($length < 5)
        $frameEvery = 1; // short clip -- frame every second
      else if ($length < 120)
        $frameEvery = 5; // short film -- frame every 5 seconds
      else if ($length > 900)
        $frameEvery = 60; // movie is over 15 minutes -- make a frame every minute
    }
    

    if ($length  &&  $maxThumbs > 0)
    {
      $frameEvery2 = ($length / $maxThumbs);
      if ($frameEvery2 > $frameEvery)
      {
        $frameEvery = $frameEvery2;
        echo "NOTE: dropping to 1 frame every $frameEvery seconds.\n";
      }
    }
    // NOTE: $frameEvery may be a float not an int!


    $altxxx=false;
    if ($timestampBaseName  &&  basename($file)=='washed.ts')
    {
      $frameLimit = 666666;
      $maxThumbs = $length * 3;
      $frameEvery = 1;
      $altxxx = true;
    }
    
    
    $rate = 1 / $frameEvery;
    echo("\n    Thumbnail maker: length=".Util::duration($length, 1).
         "  =>  Thumb every $frameEvery seconds (rate:$rate)\n\n");

    // cmd to make the thumbnails
    $frameLimit = "-vframes $frameLimit";
    if ($rate < 1)
    {
      // newer ffmpeg doesn't seem to be able to handle a rate < 1 w/o a semi
      // arbitrary "-b"(itrate) arg
      $rate .= " $xtraArgs ";
    }



    $cmd = "$ffmpeg -i ".Util::esc($file).
      ($altxxx ? " -an $frameLimit -r $rate -sameq $outDir/%08d.jpg" :
       " -an $vf $frameLimit -r $rate $outDir/%08d.jpg");
      

    // normally we'd like to kill/timeout after 1 hour.
    // however aug2009, /details/scipy09_advancedTutorialDay1_4
    // that was too short.
    //
    // So, if we *know* the video is quite long, scale up 1.5x in hours
    //   1 hr ==> 1.5 hr timeout
    //   2 hr ==> 3   hr timeout
    //   3 hr ==> 4.5 hr timeout
    // (min 1 hours for timeout)
    $maxtime = Util::timeoutTime($length, '1.5x', '1h');

    // TV archive should red row if fails...
    if (Util::cmdQT($cmd, $maxtime, 'WARN'))
    {
      // fail.  some items have so much damage they can fail deep into the
      // extraction and segfault ffmpeg when deinterlace-ing, eg:
      //    /details/TELECONGO_20110812_190000
      // so try one more time w/o the deinterlacing...
      $cmd = Video::yadif_nix($cmd);
      if (Util::cmdQT($cmd, $maxtime, ($tvarchive ? 'THROW':'WARN')))
        return null; // ffmpeg failed or, more likely, timed out!
    }
    
    $thumbnails = glob("$outDir/*.jpg");
    sort($thumbnails);

    
    if (!count($thumbnails))
      throw new Exception("No thumbnails made");


    

    // Mar2007 -- switch from mplayer to ffmpeg
    // an improvement!  however, strangely, the first interval of the film
    // always seems to have 2 thumbs, not 1 (as it should) and they are often
    // "nearly the same".  so let's remove thumbnail frame ..0001
    if (count($thumbnails) > 1)
    {
      `rm -f {$thumbnails[0]}`;
      unset($thumbnails[0]);
    }
    

    $timestamps = false;
    if ($timestampBaseName  &&  $length  &&  $frameEvery > 1.5)
    {
      $expected = ($length / $frameEvery);
      if (($tvarchive  &&  $expected > 5)  ||
          ($expected > 1  &&  abs(count($thumbnails) - $expected) <= 2))
      {
        $timestamps = true;
        echo "\n    Will use timestamps for created thumb filenames\n";
      }
      else
      {
        echo "\n    Not using timestamps -- expected:$expected; made:".
          count($thumbnails)."\n";
      }
    }

    if (!$length  &&  $maxThumbs > 0  &&  count($thumbnails) > $maxThumbs)
    {
      echo "NOTE: Thinning out the ".count($thumbnails)." thumbnails to $maxThumbs\n";
      $thumbnails = Util::sampleList($thumbnails, $maxThumbs);
    }
    
    if ($maxThumbs > 0  &&  count($thumbnails) > $maxThumbs)
    {
      // for a movie with no determined length (so we used "every 30 seconds")
      // we may have made too many frames.
      // take the first $maxThumbs slices (and throw out the rest)
      echo "NOTE: Made too many!  skipping 1st, then taking next $maxThumbs (of ".
        count($thumbnails).") frames.\n";
      $thumbnails = array_slice($thumbnails, 1, $maxThumbs+1);
    }


    if ($timestampBaseName)
    {
      $first=true;
      $i=0;
      $ary = $thumbnails;
      $thumbnails = array();
      foreach ($ary as $thumbnail)
      {
        // if we are using timestamps, use timecode (seconds) from start for name
        // else make 1st thumb ..001; 2nd ..002; etc...
        if ($timestamps)
        {
          if ($i==0)
          {
            $sec = 1;
          }
          else
          {
            $sec = Video::framenum2sec($file, $thumbnail, $identify);
            if ($sec < 0)
              $sec = round($i*$frameEvery);
          }
        }

        $num = ($timestamps ? $sec : $i+1);

        echo basename($thumbnail)." ==> $num\n";
        
        $thumbFile = $timestampBaseName.sprintf("%06d", $num).'.jpg';
        Util::cmd("mv $thumbnail ".Util::esc($thumbFile), ($altxxx ? "CONTINUE" : "THROW"));
        // NOTE: subtle!  2+ frames *could* end up being later considered
        // the same filename, eg: /details/ShaneBrennanSittingTourBreathe
        // so we'll pick the *last* one when 2+ frames converge on same name
        $thumbnails[$num] = $thumbFile;
        $i++;
      }
    }
    
    
    return array_values($thumbnails);
  }

  

  // first two parameters are the *target* video dimensions
  private static function vfilters($width, $height, &$identify, $tvarchive,
                                   $h264, $paddingTB=0, $paddingLR=0)
  {
    //echo "VF:$width $height $tvarchive $paddingTB $paddingLR\n";
    
    if ($h264  &&  $width==426  &&  $height==240)
    {
      // OK, this is quite a bit weird and nonobvious....
      // For h.264, we'd ideally like to "fit within" the "level 1.3"
      // which has a limit on the number of "macroblocks/second".
      // that is simply the (width/16 * height/16 * FPS)
      // sooo.... for a 16x9 widescreen 29.97 fps film, we would normally
      // try to resize it to 426 x 240.  but that has 2 problems.
      // A) [ceil(426/16)=27] * [ceil(240/16)=15] * 29.97 ==> 12,138
      //     which is over the max of 11880 macroblocks/sec for level 1.3
      // B) 426 is not a multiple of 16 (makes h.264 less compressed/ideal)
      // (although theoretically we can live with that)
      //
      // whereas if we cheese it to 416 (nearest smaller multiple of 16)
      // we solve both problems w/ arguably unnoticeable squooshing
      //
      // there's a nice table here about profiles-n-macroblocks:
      /*   http://en.wikipedia.org/wiki/H.264#Levels  */
      $width = 416;
    }
    

    // for TV archive movies -- we want to remove the top 4 pixels of the video
    // which frequently contain programming information "hidden" in them, making
    // them look like static at the top of the image
    $topclip = $topclipScaled = 0;
    if ($tvarchive)
    {
      $topclip = 4; // Make sure always a multiple of 2!!

      if (!$identify['width']  ||  !$identify['height'])
        fatal(TV::$ERRS[2]);

      // Say for example we are taking a 480p height video and making a 110px
      // high thumbnail.  that's a downscale of (over) 4:1.
      // We will always run the "crop" filter first, and "scale" filter next.
      // The "scale" filter takes the resulting *target height*, so we need
      // to also scale the top clip height appropriately, too...
      $topclipScaled = round($topclip * (1.0 * $height / $identify['height']));
      error_log("topclip:$topclip, height:$height, filmHeight:{$identify['height']} topclipScaled:$topclipScaled");
    }

    // dimens minus any black bar padding, and minus any top clipping 
    $nonBlackHeight = $height-(2*$paddingTB)-$topclipScaled;
    $nonBlackWidth  = $width -(2*$paddingLR);

    // this is the position inside the overall $width X $height position the
    // visible video pixels will start at
    $padYstart = $paddingTB + ($topclip ? max(1,round($topclipScaled/2)) : 0);
    $padXstart = $paddingLR;


    
    $filters = array();

    // hmm, this *helps* for Thumbnails and tricky MPEG-TS items, but
    // not reliable enough for mp4 for some shows, eg:
    //   /details/MSNBCW_20120502_010000_The_Rachel_Maddow_Show
    if ($tvarchive  &&  !$h264) $filters[] = 'setpts=PTS-STARTPTS';


    // OK, it's prolly more ideal for majority of videos to do this:
    //   $filters = array('yadif=0:-1:1');
    // and *not* deinterlace if the source is progressive (or claims to be).
    // However, for videos that have effectively "burned in" a bad interlace
    // into a progressive frame (eg: got the top/bottom field first wrong)
    // the output is nearly unwatchable.  So deinterlace alwayz...
    //   Eg:  /details/dom-25917-watchthisshow-digitalspin1996
    $filters[] = 'yadif';

    
    

    // Respect iphone4+ especially, as an example.
    // eg:  /details/farmlandllp2012
    if ($identify['rotation']==90)
      $filters[] = 'transpose=1';
    else if ($identify['rotation']==180)
      $filters[] = 'vflip=1,hflip=1';
    else if ($identify['rotation']==270)
      $filters[] = 'transpose=2';
    
    if ($topclip)
      $filters[] = "crop={$identify['width']}:".($identify['height']-$topclip).":0:$topclip";

    $filters[]   = "scale=$nonBlackWidth:$nonBlackHeight";

    if ($topclip  ||  $paddingTB  ||  $paddingLR)
      $filters[] = "pad=$width:$height:$padXstart:$padYstart:black";

    //printr($filters);
    
    return array('-vf '.join(',',$filters), $width, $height);
  }
  
  
  // some video files have non-4:3 aspect ratio (w/ "square"/normal pixels).
  // some video files have non-4:3 aspect ratio (w/ "rectangular" pixels) like
  //    DV video or anamorphic mpeg2 dvds. some of those files have 4:3 aspect
  //    ratio BUT have "rectangular pixels")
  // this method tries to sort out what the "desired visual width and height"
  // is for the input clip.
  //
  // Let's say the "desired visual width and height" is "-s 320x240".
  //
  // If padBlack is false:
  //   if the clip is of 4:3 ratio, we'll return
  //      "-s 320x240"
  //   if the clip isn't 4:3 ratio, (eg: 16:9) we'll return a stretched version:
  //      "-s 428x240"
  //
  // If padBlack is true:
  //   if the clip is of 4:3 ratio, we'll return
  //      "-s 320x240"
  //   if the clip isn't 4:3 ratio, (eg: 16:9) we'll return a version that
  //   has "black bars" on the top and bottom:
  //      "-s 320x180 -vf pad=..."
  public static function vf($targetScale,
                            $identify,
                            $tvarchive=false,
                            $padBlack=false,
                            $h264=false)
  {
    if (!preg_match('/^\-s (\d+)x(\d+)$/', $targetScale, $mat))
      fatal("bad input param!");
    $targetWidth = $mat[1];
    $targetHeight= $mat[2];


    if (isset($identify['width'])  &&  isset($identify['height']))
    {
      $filmWidth = $identify['width'];
      $filmHeight= $identify['height'];


      // if pixels are rectangular, stretch the width to same size
      // in square pixel equivalents
      if (preg_match('/(\d+):(\d+)/', $identify['PAR'], $mat))
      {
        // latest version of ffmpeg (finally!) outputs Pixel Aspect Ratio, yay!
        $parW = $mat[1];
        $parH = $mat[2];

        // echo "\nASPECT $parW / $parH\n\n";
        if ($parW > 0  &&  $parH > 0)
          $filmWidth = round($filmWidth * $parW / $parH);
      }
      


      

      // if film's actual width/height ratio is equal (or close -- 90%+ same)
      // to the desired width/height (typically 4x3)
      // then keep desired width/height AS IS
      $r1 = $filmWidth   / $filmHeight;
      $r2 = $targetWidth / $targetHeight;
      $ratiopercents = min($r1,$r2) / max($r1,$r2);
      // echo "ratiopercents: $ratiopercents\n";
      if ($ratiopercents < 0.9)
      {
        // film's ratio of width/height is far enough off from the target
        // visual window's width/height ratio that we should rescale the film
        $newTargetWidth = $targetHeight * $filmWidth / $filmHeight;
        // echo "NTW: $newTargetWidth\n";

        if ($newTargetWidth >= 2048  &&  $targetHeight >= 300)
        {
          // we'd make a *hella* wide video.  so let's try for 240p and see if
          // we can fit it in there at the same wide aspect instead....
          $targetWidth  = round($targetWidth/2);
          $targetHeight = round($targetHeight/2);
          return Video::vf("-s {$targetWidth}x{$targetHeight}",
                           $identify, $tvarchive, $padBlack, $h264);
        }
                
        if ($newTargetWidth >= 50  &&  $newTargetWidth < 2048)
        {
          if ($padBlack)
          {
            // we want to add "black bars"...
            if ($newTargetWidth >= $targetWidth)
            {
              // we want to add "black bars" on the top and bottom...
              $newTargetHeight = round($targetWidth * $filmHeight / $filmWidth);
              // echo "PADDING: $newTargetHeight $targetHeight\n";
            
              $padding = Video::roundToMultipleOf2(
                ($targetHeight - $newTargetHeight) / 2);
            
              return Video::vfilters($targetWidth, $targetHeight, $identify,
                                     $tvarchive, $h264, $padding);
            }
            else
            {
              // we want to add "black bars" on the left and right...
              // rare narrow (width < height) video!
              // 
              // -vf scale=74:110,pad=160:110:43::black
              $newTargetWidth = Video::roundToMultipleOf2($newTargetWidth);

              $padding = Video::roundToMultipleOf2(
                ($targetWidth - $newTargetWidth) / 2);
              
              // echo "$newTargetWidth x $targetHeight $padding \n";
              return Video::vfilters($targetWidth, $targetHeight, $identify,
                                     $tvarchive, $h264, 0, $padding);
            }
          }
          else
          {
            // we want to stretch out the created derivative film's width
            $newTargetWidth = Video::roundToMultipleOf2($newTargetWidth);
            return Video::vfilters($newTargetWidth, $targetHeight, $identify,
                                   $tvarchive, $h264);
          }
        }
      }
    }

    return Video::vfilters($targetWidth, $targetHeight, $identify, $tvarchive,
                           $h264);
  }


  // makes sure the returned value is a multiple of 2
  // (the closest multiple of 2 ;-) and an integer
  public static function roundToMultipleOf2($val)
  {
    if (round($val) % 2 == 0)
      return round($val);

    $diffA = round($val - 1) - $val;
    $diffB = round($val + 1) - $val;
    return round($val + ($diffA < $diffB ? -1:1));
  }
  
  
  // just like min() -- but handle comparison if either/both terms have
  // postpended "k" to their float to be treated like "times 1000"
  public static function min($a, $b)
  {
    $aa = (rtrim($a,'k')==$a ? $a : $a * 1000);
    $bb = (rtrim($b,'k')==$b ? $b : $b * 1000);
    if ($aa < $bb)
      return $aa;
    else
      return $bb;
  }


  // start at first I-frame (eg: MPEG-TS and MPEG-PS)
  public static function seek_to_first_keyframe($cmdpipe, $seekMaxB=1000000)
  {
    $ffprobe = configGetPetaboxPath('bin-ffprobe');
    
    // NOTE: make sure that we skip any "first keyframe" that (bizarrely) comes up
    // a **negative** byte position.
    $first_byte_for_first_keyframe = Util::cmd("( $cmdpipe | head -c $seekMaxB | $ffprobe -show_packets - 2>/dev/null | perl -ne 'chop unless m=/PACKET=; print;' | perl -ne 'next unless m/codec_type=video/; next unless m/flags=K/; next unless m/ pos=(\d+)/; print \$1; exit;' ) 2>/dev/null","CONTINUE","PRINT");

    return ($first_byte_for_first_keyframe ? 
            " | tail -c +$first_byte_for_first_keyframe " : "");
  }
  

  private static function ffmpeg_vf(&$identify,
                                    $wh="640x480", $is_tv=false, $h264=false)
  {
    // for "rectangular pixel" video -- "stretch" width to square pixels
    list($vf,$outW,$outH) = Video::vf("-s $wh", $identify,
                                      $is_tv,
                                      /*padBlack==*/false,
                                      $h264);
    if ($h264  &&  $wh=='640x480'  &&  $outW > 640)
    {
      // OK, for widescreen 16x9 videos, they'll expand to 852x480
      // which is *fine*, but at least for iPhone 1st/2nd gen, they fail
      // to play videos wider than 640 pixels.
      // So scale it back down to 360P at least for now...
      
      list($vf,$outW,$outH) = Video::vf('-s 480x360', $identify,
                                             $is_tv,
                                             /*padBlack==*/false,
                                             $h264);
    }
    
    return $vf;
  }
  

  public static function ffmpeg_params($cmd, &$identify, $sourceFormat, $acoder,
                                       $params="640x480", $is_tv=false, $h264=false)
  {
    if (!$identify['width']  ||  !$identify['height'])
    {
      if ($is_tv)
        fatal(TV::$ERRS[2]);
      else
        fatal("Video width/height not calculable -- likely no video track!");
    }

    if (!preg_match("/^(\d+x\d+)(.*)/", $params, $mat))
      fatal("bad params! [$params]");
    list(,$wh,$rest) = $mat;
    
        


    $vf = Video::ffmpeg_vf($identify, $wh, $is_tv, $h264);
    
    
    $rate = ($identify['fps'] ? " -r {$identify['fps']} " : "");
    //$rate = "";//xxxx


    $cmd .=
      " $vf $rest $rate -threads 2 ".
    
      // http://webcache.googleusercontent.com/search?q=cache:m16Nv-PyqZsJ:ffmpeg-users.933282.n4.nabble.com/DVB-transports-streams-a-v-line-up-sort-of-like-projectx-does-td1009651.html+ffmpeg+%22dts_delta_threshold%22&cd=4&hl=en
      ($identify['audio']!=='false'  && 
       ($is_tv  ||  
        in_array($sourceFormat, array('MPEG2','MPEG1','h.264/MPEG2-TS')))
       ? Video::AV_ANTI_DRIFT : "").
            
      (isset($identify["DAR"]) ? " -aspect {$identify['DAR']}" : "").
      " -map_metadata -1,g:0,g";

    
    // normally, we'd be ready to run!  however, quick test first second...
    {
      $tmpf = "/tmp/rawvidtmp.".getmypid().".avi";
      $retval = Util::cmdQT("$cmd -an -t 1.0 -y $tmpf", 0.5, 'WARN');
      `rm -f $tmpf`;
      if ($retval)
      {
        // OK this can *also* fail as of (oneiric) Nov2011 when also using
        // "baseline" profile (which we do!), if source is yuv 422, eg:
        //    /details/AlcTestClip
        // or source is yuv 444, eg:
        //    /details/SamsFrames
        $cmd .= ' -pix_fmt yuv420p ';
      }
    }




    // now figure out the audio params
    if ($identify['audio']  &&  $identify['audio'] != 'false')
    {
      // for archive items (esp. TV), always use 1st audio track IFF 2+
      $map = (isset($identify['astreams'])  && 
              isset($identify['vstreams']) ?
              ' -map '.str_replace('.',':',strtok($identify['vstreams'],',')).
              ' -map '.str_replace('.',':',strtok($identify['astreams'],',')) : '');
      $acoder = "$map $acoder";
    }
    else
    {
      // no audio!
      $acoder = "-an";
      if ($is_tv)
      {
        // rare, but /details/KICU_20110322_110000_Paid_Programming ...
        echo "\nWARNING! NO CHANNELS FOUND IN AUDIO STREAM(S)!\n\n";
      }
    }
    
    
    
    
    return array($cmd, $acoder);
  }
  
  
  public static function regress($all=false)
  {
    assert_options(ASSERT_CALLBACK, array('ConfigTest', 'assert_callback'));
    assert_options(ASSERT_BAIL, 1);
    errtesting();
    
    $tests = array(

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

      );
    
    foreach ($tests as $test => $shouldB)
    {
      $meta = json_decode($test);
      $item = new Item($meta->identifier);
      $item->meta = $meta;
      $item->filesInfo = $meta->filesInfo;
      $meta->mainDir='/';
      foreach ($meta->filesInfo as $filocation => &$fi)
        $fi = (array) $fi;
      
      list($groups, $nGroupsAV, $showing, $captions)=self::filesToGroupsAV($item, $meta->filesInfo);
      printr($groups);
      assert(json_encode($groups) == $shouldB);
    }

    if (!$all)
      return;
    

    // now test "fps" frame rates
    error_log("Checking frame rates");
    chdir("/home/tracey/public_html/_/") || fatal("no test vid dir!");
    foreach (array(
    '007333/007333.asf' => 14.99,
    '06ConclusionDaAnterir/05ReflexionPonenciaJoseMurilo2.mp4' => 29.97,
    '06ConclusionDaAnterir/06ConclusionesDaAnterior..mp4' => 29.97,
    '070125_chantier/070125prisesext1fence.mov' => 29.97,
    '170109perLaPalestina/170109corteopalestina.mov' => 25,
    'AAC-NON-MONOTONE/Xacti1010_trim2_test_2010_02_04.mp4' => 29.97,
    'AlcTestClip/AlcTestClip.mov' => 29.97,
    'Alivega427-BasicTraditionalPackage332/Alivega427-BasicTraditionalPackage332.mov' => 29.97,
    'AresQuarterlyProgressReport11/QPR11.m4v' => 29.97,
    'BayrampasaVaillantServisi444_1_494VaillantServisi_1000/8.flv' => 23.98,
    'Bij_de_Italiaan/Italiaan.3gp' => 15,
    'BraveNewFilmsFoxAttacksObama/foxattacksobama320.mov' => 29.97,
    'BrokeNProud/Bnp_finalCut_intro_explicit_720.mov' => '',
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
    'Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805/Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler252.flv' => 15,
    'Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805/Drringell-SamtalMedIngvarKullbergOmMandelbrotsfraktaler805.3GP' => 15,
    'EM/EM-0028-01.mov' => 30,
    'EchoAlert009/Ea009.m4a' => 0.01,
    'ExcerptFromASacredDuty/asd.avi' => 29.97,
    'FreeHdStockFootageTimeLapseOfHighwayTrafficAtDusk/FreeStockFootageTimeLapseOfHighwayTrafficAtDuskInHdFromClipcanvas.mov' => 25,
    'FreedomProjectMovieAndTvSeries-MissionToMir/Mission_to_mir.mov' => 14.97,
    'GreatCitiesDialogue/GreatCities_1.mp4' => 29.97,
    'GreekTelevisionTest/greece-test.mpg' => 25,
    'GrowingYounger/JonAizen-GrowingYounger.mp3' => '',
    'HAMZA-813/87.rmvb' => '',
    'HappyBirthday/HappyBirth.mpg' => 30,
    'HoChak/8tvHoChat.mov' => 15,
    'Holymatrimony/holymatrimony.wmv' => 23,
    'IbisBlancs/ibisblancs.wmv' => 30,
    'IntroToSimedit/Screencap.mov' => 30,
    'Jessies_Sunrise/Jessies_Sunrise.mpg' => 29.97,
    'KOFY_20110607_093000_Paid_Programming/KOFY_20110607_093000_Paid_Programming.mpg' => 29.97,
    'KQED_20100820_150000_Curious_George/KQED_20100820_150000_Curious_George.mpg' => 29.97,
    'KQED_20100922_050000_POV/KQED_20100922_050000_POV.mpg' => 29.97,
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
    'Podcast7_492/Podcast7.m4a' => 6,
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
    'The_Phantom_Empire_1935/The_Phantom_Empire_1935.mp4' => 24.36,
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
    'canon-hf-10-test-2008_09_23a/canon-hf-10-test-2008_09_23a.mts' => 59.94,
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
    'lotus-drifting/lotus-drifting.mov' => 29.97,
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
    'warmy-test/camels.avi' => 20,
    'witness_6447_E007499/fillup shots.mov_bitc.mov' => 25,
    'witness_6447_E007499/fillup shots1.mov_bitc.mov' => 25,
    'witness_6447_E007499/fillup shots2.mov_bitc.mov' => 25,
               ) as $fi => $fps)
    {
      $i = Video::identify($fi, false, 150, true);
      error_log("$fi should be:   $fps   is:   {$i['fps']}");
      assert($i['fps'] == $fps);
    }
    echo " ALL GOOD!\n";
    
    


    // now test some "ffmpeg -identify" kind of very important checks!!
    $lo = new Locator();
    foreach (
      array(
        'pews/pews.mov' => // rotated 90-degree iphone 1080P test
        '{"ac":"aac","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":1,"fps":24,"height":"1920","length":15.05,"pix":"yuv420p","rotation":"90","vc":"h264","vstreams":"0:0","width":"1080"}',

        'BananaRepublicING/04HallwaysTherapyAquarium.mp3' =>
        '{"ac":"mp3","ar":"44100","astreams":"0:0","audio":"true","audio_nchannels":2,"fps":null,"height":null,"length":727.66,"width":null,"vstreams":""}',
        
        'WUSA_20091225_000000_9News_Now_Tonight' =>
        '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":336.3,"pix":"yuv420p","vc":"mpeg2video","vstreams":"0:1","width":"528"}',

        'OvercomePt.3-OvercomeTheWorldWithLove_787/Episode15-Overcome-Pt.3-overcomeTheWorldWithLove.m4a' =>
        '{"DAR":"1:1","PAR":"72:72","ac":"aac","ar":"44100","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":6,"height":"300","length":3490.09,"pix":"yuvj444p","vc":"mjpeg","vstreams":"0:3","width":"300"}',

        'The_Phantom_Empire_1935/The_Phantom_Empire_1935.mp4' =>
        '{"ac":"aac","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"24.36","height":"480","length":4145.28,"pix":"yuv420p","vc":"h264","vstreams":"0:0","width":"640"}',




        'EchoAlert009/Ea009.m4a' =>
        '{"DAR":"1:1","PAR":"1:1","ac":"aac","ar":"44100","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":"0.01","height":"300","length":4909.46,"pix":"yuvj420p","vc":"mjpeg","vstreams":"0:3","width":"300"}',
        
        
        'ExcerptFromASacredDuty/asd.avi' =>
        '{"DAR":"4:3","PAR":"1:1","ac":"mp2","ar":"48000","astreams":"0:0","audio":"true","audio_nchannels":2,"fps":29.97,"height":"480","length":696.51,"pix":"yuv420p","vc":"mpeg4","vstreams":"0:1","width":"640"}',
        
        
        'Podcast7_492/Podcast7.m4a' =>
        '{"DAR":"1:2","PAR":"72:72","ac":"aac","ar":"32000","astreams":"0:2","audio":"true","audio_nchannels":2,"fps":6,"height":"320","length":3783.07,"pix":"yuvj444p","vc":"mjpeg","vstreams":"0:3","width":"160"}',
        

        // item deleted, so check in home dir..
        '/home/tracey/public_html/_/BayrampasaVaillantServisi444_1_494VaillantServisi_1000/8.flv' =>
        '{"audio":"false","fps":23.98,"height":"720","length":29.89,"pix":"yuv420p","vc":"vp6f","vstreams":"0:0","width":"1280","astreams":""}',
  
        
        'MITOCW1800intros/MIT_OCW_1800_OCW_intro.mp4' =>
        '{"DAR":"4:3","PAR":"8:9","ac":"aac","ar":"48000","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"29.97","height":"480","length":502.18,"pix":"yuv420p","vc":"h264","vstreams":"0:0","width":"720"}',

        'FOXNEWS_20091125_020000_Hannity' =>
        '{"DAR":"4:3","PAR":"10:11","ac":"ac3","ar":"48000","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"29.97","height":"480","length":3600.36,"pix":"yuv420p","vc":"mpeg2video","vstreams":"0:2","width":"704"}',
        
        'commute/commute.avi' =>
        '{"ac":"mp3","ar":"44100","astreams":"0:1","audio":"true","audio_nchannels":2,"fps":"10","height":"480","length":115.59,"pix":"yuvj420p","vc":"mjpeg","vstreams":"0:0","width":"640"}',
        
        'WUSA_20110519_223000_CBS_Evening_News_With_Katie_Couric' =>
        '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":1800.33,"pix":"yuv420p","vc":"mpeg2video","vstreams":"0:1","width":"528"}',

        'WUSA_20091019_160000_9News_Now_at_Noon' =>
        '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"29.97","height":"480","length":1800.73,"pix":"yuv420p","vc":"mpeg2video","vstreams":"0:1","width":"528"}',

        'WUSA_20091118_140000_9News_Now_at_9am' =>
        '{"DAR":"4:3","PAR":"40:33","ac":"ac3","ar":"48000","astreams":"0:2,0:3","audio":"true","audio_nchannels":4,"fps":"59.92","height":"480","length":73289.89,"pix":"yuv420p","vc":"mpeg2video","vstreams":"0:1","width":"528"}',
        
        ) as $idish => $json)
    {
      $ary = json_decode($json,1);
      foreach ($ary as $k=>&$v)
      {
        if ($v==="")
          unset($ary[$k]);
      }
      
      if (strncmp($idish,'/home/',6)===0)
      {
        $id = '';
        $fi = ltrim($idish,'/');
        $r = array('home','/');        
      }
      else
      {
        list($id,$fi) = explode('/',$idish,2);
        if (!$fi)
          $fi = "$id.mpg"; // TV
      
        $r = $lo->locateUDP($id,1,1);//find primary copy
        $r = $r[0];
      }
      
      foreach (array("/petabox", "/home/tracey/petabox") as $pethome)
      {
        error_log("CHECKING $id/$fi ($pethome)");
        
        $scr = "
export PETABOX_HOME={$pethome};
php -r 'require_once(\"/petabox/setup.inc\"); echo json_encode(Video::identify(\"{$r[1]}/{$fi}\"));'
";
        file_put_contents('/home/tracey/bleah',$scr);
        
        $str = Util::cmd("ssh root@{$r[0]} bash /home/tracey/bleah");//"PRINT");
        $ffi = json_decode($str,1);
        ksort($ffi);

        unset($ffi['ffmpeg-version']);
        if ($ary != $ffi)
        {
          error_log("NOT EQUAL!");
          error_log("--SHOULD BE:");          
          printr($ary);
          error_log("--CURRENTLY IS:");          
          printr($ffi);
          error_log(json_encode($ffi));
          assert(false);
        }
      }
    }
    unlink('/home/tracey/bleah');
  }
}

?>
