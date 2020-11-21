<?

class MPEG4 extends Module {
  const ACODER = '-c:a libfdk_aac -vbr 3 -ac 2';

  const VCODER_BASE = '-profile:v main -crf 23 -preset:v fast -x264-params ';
  const VCODER_SD = self::VCODER_BASE . 'level=30:bitrate=700:vbv_maxrate=768:vbv_bufsize=1400';
  const VCODER_HD = self::VCODER_BASE . 'level=31:bitrate=2048:vbv_maxrate=2250:vbv_bufsize=4096';

  const ARGS = '-movflags faststart';

  private static $already_ran = [];


  public function derive($src = null, $dest = null, $identify = null, $first = true, $title = 1) {
    if ($this->sourceFormat == 'Metadata')
      return $this->popcornproject($src ?: $this->sourceFile);


    if ($src == null)
      return $this->deriveVideo(); // calls self w/ args set, ISO Image preprocessing if needed


    chdir($this->tmp)  ||  fatal('unable to change to temp dir');

    $ffmpeg = Video::ffmpegV0($src);


    if ($this->is_tvarchive()  &&  TV::is_audio_only($this->identifier, $identify))
      return;

    $HD = $this->targetFormat === 'h.264 720P';

    $args = join(' ', [
      $HD ? '1280x720' : '640x480',
      $HD ? self::VCODER_HD : self::VCODER_SD,
      self::ARGS,
    ]);

    if (self::$already_ran[$src])
      return;
    self::$already_ran[$src] = true;

    if ($this->targetFormat === 'h.264 IA') {
      if (!$this->user_mp4_needs_full_transcode($src, $dest, $identify))
        return;
    }


    $times3 = Util::timeoutTime($identify,  '3x',  '4h');
    $timesX = Util::timeoutTime($identify, '10x', '20h');


    // metadata inserter
    $meta = Video::videoMeta($this, ['title'         => '-metadata title=',
                                     'date'          => '-metadata year=',
                                     'director'      => '-metadata author=',
                                     'licenseurl'    => '-metadata comment=license:',
                                    ], true);

    $cmd = "$ffmpeg -y -i " . Util::esc($src) .' -c:v libx264';


    // chrome browser seems to have stopped supporting yuvj420p (eg: motion-JPEG)
    // colorspace as of start of 2012...
    // another example yuvj422p  /details/babyelephant
    if (preg_match('/yuvj...p/', $identify['pix']))
      $cmd .= ' -pix_fmt yuv420p'; // ...so convert it

    if ($HD) {
      // making HD deriv -- verify (for now, for certain collections) that *source* is at least HD...
      $collex = (array)($this->meta_xml()['collection']);
      if (in_array('internetarchivepresents', $collex)  &&
          !FormatGetter::isHD($identify['width'], $identify['height'])) {
        echo "source video is only {$identify['height']}P -- skipping this 720P derivative\n";
        return;
      }
    }


    $tmp = 'tmp.mp4';
    echo "\n";

    list($tries, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat, self::ACODER, $args, $this->is_tvarchive(), true);

    foreach ($tries as $trie) {
      $err = Util::cmdQT("$trie $acoder $meta $tmp", $timesX, 'CONTINUE');
      echo "\n";
      if (!$err)
        break;
      echo "FAILED\n";
    }

    if ($err)
      fatal('MPEG4 FAILED ALL ATTEMPTS');


    Util::cmdQT("mv $tmp " . Util::esc($dest));

    $this->copyOrigMeta(); // backfills in any current metadata information
  }


  /**
   * Probes an user-original .mp4 upload to see if it's _likely_ web-playable.
   * If seems playable, we can skip the full derivative altogether and use it in /details/ theatre.
   * If not, adjust to the minimal amount of transcoding to actually create the derivative version.
   *
   * @param string $src  Source filename
   * @param string $dest  Dest filename
   * @param array $identify  Hashmap of video characteristics
   * @return boolean  true if should do entire transcode to new .mp4
   */
  private function user_mp4_needs_full_transcode($src, $dest, $identify) {
    if ($this->targetFormat === 'h.264 IA'  &&  !Util::ends_with($src, '.mp4'))
      return false;

    if ($identify['vc'] !== 'h264'  ||  $identify['pix'] !== 'yuv420p')
      return true;

    // allow user original mp4 <= 10mb/s files
    $mbps = ($identify['bytes']  &&  $identify['length']
       ? round($identify['bytes'] * 8 / 1024 / 1024 / $identify['length'], 1)
       : 0);
    echo basename($src) . " is ~${mbps}mb/s\n";
    if ($mbps > 10)
      return true;

    $tmp = 'tmp.mp4';

    if ($identify['ac'] === 'aac') {
      // now check if moov atom is at the front
      if (FormatGetter::mp4_has_moov_at_end($src)) {
        echo basename($src) . " seems web safe\n";
        return false; // we should be able to use .mp4 original AS IS in /details/ page
      }

      // OK, user original is _likely_ OK but just needs moov atom at start
      $err = Util::cmdQT(
        Config::value('bin-qt-faststart') . ' ' . Util::esc($src) . " $tmp",
        Util::timeoutTime($identify, '1x', '4h'),
        'CONTINUE'
      );
      if (!$err) {
        Util::cmdQT("mv $tmp " . Util::esc($dest));
        return false; // all good - new quickly made derivative file _should_ be playable
      }
    }

    // make the video processing portions copy
    $param = ' -c copy ' . self::ARGS;

    $err = Util::cmdQT(
      Video::ffmpegV0($src) . ' -y -i ' . Util::esc($src) . " $param " . self::ACODER . " $tmp ",
      Util::timeoutTime($identify, '1x', '4h'),
      'CONTINUE'
    );
    if (!$err) {
      Util::cmdQT("mv $tmp " . Util::esc($dest));
      return false; // all good - new quickly made derivative file _should_ be playable
    }

    return true;
  }


  public function popcornproject($src) {
    // This is a special Popcorn (see archive.org/pop ) project that has been saved to archive.org by an editor
    // We will now make a static single .mp4 from the project JSON EDL.

    if ($src === NULL  ||  !Util::ends_with($src, '_meta.xml'))
      return; // we don't want a derivative linked to the _files.xml

    if (self::$already_ran[$src])
      return;
    self::$already_ran[$src] = true;

    // extract the JSON project EDL from _meta.xml
    $mxml = $this->meta_xml();
    if (!isset($mxml['shotlist']))
      return; // not a popcorn project item _in actuality_

    $json = $mxml['shotlist'];
    chdir($this->tmp)  ||  fatal('unable to change to temp dir');
    //printr($json);
    $fi = "{$this->identifier}.json";
    file_put_contents($fi, $json);
    //file_put_contents("{$this->itemDir}/{$fi}", $json); //xxx extraTarget ?!
    Util::cmdPP(
      //phpcs:ignore
      'env TERM=vt100 MAGICK_CONFIGURE_PATH=/opt/popcorn-exporter FFMPEG_BINARY=/usr/bin/ffmpeg python2 /opt/popcorn-exporter/convert.py ' .
      Util::esc($fi). ' tmp.mp4'
    );

    Util::cmdQT('qt-faststart  tmp.mp4  tmp2.mp4');
    unlink('tmp.mp4');

    $dest = "{$this->itemDir}{$this->identifier}.mp4";

    Util::cmdQT('mv tmp2.mp4 ' . Util::esc($dest));

    $this->extraTarget($this->shortName($dest), 'h.264 HD', true);
  }
}
