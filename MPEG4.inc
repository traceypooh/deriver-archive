<?
require_once('Module.inc');

class MPEG4 extends Module
{
  const ACODER = '-acodec aac -strict experimental';
  
  
  public function version()
  { return '$Revision: 43990 $ $Date: 2012-05-18 01:32:34 +0000 (Fri, 18 May 2012) $'; }

  
  public function derive()
  {
    if ($this->sourceFormat == 'ISO Image')
      $this->deriveFilesISO();
    else
      $this->deriveFile();
  }



  public function deriveFile($src=null, $dest=null, $identify=null)
  {
    if (!$src)    $src = $this->sourceFile;
    if (!$dest)   $dest= $this->targetFile;
    
    chdir($this->tmp)  ||  fatal("unable to change to temp dir");

    $ffmpeg             = configGetPetaboxPath('bin-ffmpeg').' -v 0';
    $qtfaststart        = configGetValue('bin-qt-faststart');

    
    // first, print clip info to log (since we'll throw out all ffmpeg output)
    if (!$identify)
      $identify = Shared::identify($src, $this->is_tvarchive());


    $times3 = Util::timeoutTime($identify,  '3x',  '4h');
    $timesX = Util::timeoutTime($identify, '10x', '20h');

    
    // metadata inserter
    $meta = Shared::videoMeta($this, array(
                                'title'         => '-metadata title=',
                                'date'          => '-metadata year=',
                                'director'      => '-metadata author=',
                                'licenseurl'    => '-metadata comment=license:',
                                ), true);

    $cmd = "$ffmpeg -y -i ".Util::esc($src)." -vcodec libx264";

    // chrome browser seems to have stopped supporting yuvj420p (eg: motion-JPEG)
    // colorspace as of start of 2012...
    // another example yuvj422p  /details/babyelephant
    if (preg_match('/yuvj...p/',$identify['pix']))
      $cmd .= " -pix_fmt yuv420p"; // ...so convert it

    $params = str_replace(' /petabox/', " {$_SERVER['PETABOX_HOME']}/", $this->parameter);



    list($cmd, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat,
                           self::ACODER." -ab 128k -ac 2 -ar 44100",
                           $params, $this->is_tvarchive(), true);
    
    
    $tmp = 'tmp.mp4'; echo "\n";

    $p1 = "-pass 1 -an";
    $p2 = "-pass 2 $acoder $meta";

    // This item failed pass 1 but worked when removed the more experimental
    // efforts to avoid A/V drift...
    //    /details/KICU_20110602_153000_Heidi_Klums_Skin_Secrets
    // NOTE: /details/KSTS_20110606_020000_Las_Cronicas_de_Spiderwick failed
    // pass 1 *sporadically* but passed 1x; also passed w/ "-deinterlace" nixed
    // (we shifted to yadif so possibly worth a shot to retry w/o...)
    $tries = array();
    $tries[] = "$cmd $p1 $tmp";
    $tries[] = str_replace(Video::AV_ANTI_DRIFT, '',     $tries[count($tries)-1]);
    $tries[] = Video::yadif_nix($tries[count($tries)-1]);
    $tries = array_values(array_unique($tries)); // nix dupes; retains order!
    //printr($tries);
    

    
    foreach ($tries as $cmd)
    {
      // now let's get pass1 done...
      $err = Util::cmdQT($cmd, $timesX, 'CONTINUE');echo "\n";
      if ($err)
      {
        echo "FAILED\n";
      }
      else
      {
        // pass1 worked, this "winning" $cmd will be used for pass 2...
        $cmd = str_replace($p1, $p2, $cmd);
        $err=Util::cmdQT($cmd, $timesX, 'CONTINUE');echo "\n";
        if ($err)
          echo "FAILED\n";
        else
          break;
    
        // OK, pass 2 can fail, likely the audio and/or audio/video matching up
        // since it's pass 2.  This item had this issue and worked when
        // removed the more experimental efforts to avoid A/V drift...
        //    /details/KOFY_20110607_093000_Paid_Programming
        $cmd = str_replace(self::ACODER, '-acodec libfaac',
                           str_replace(Video::AV_ANTI_DRIFT, '', $cmd));
        $err=Util::cmdQT($cmd, $timesX, 'CONTINUE');echo "\n";
        if ($err)
          echo "FAILED\n";
        else
          break;
      }
    }
    
    if ($err)
      fatal("MPEG4 FAILED ALL ATTEMPTS");
    
    
    Util::cmdQT("$qtfaststart $tmp ".Util::esc($dest),$times3);

    $this->copyOrigMeta(); // backfills in any current metadata information
  }
}
?>
