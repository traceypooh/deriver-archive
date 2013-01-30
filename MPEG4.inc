<?
require_once('Module.inc');

class MPEG4 extends Module
{
  const ACODER = '-acodec aac -strict experimental';
  
  
  public function version()
  { return '$Revision: 48923 $ $Date: 2013-01-17 05:54:51 +0000 (Thu, 17 Jan 2013) $'; }

  
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
      $identify = Module::identify($src, $this->is_tvarchive());

    if ($this->is_tvarchive()  &&  TV::is_audio_only($this->identifier, $identify))
      return;


    $times3 = Util::timeoutTime($identify,  '3x',  '4h');
    $timesX = Util::timeoutTime($identify, '10x', '20h');

    
    // metadata inserter
    $meta = Video::videoMeta($this, array(
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


    list($tries, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat,
                           self::ACODER,
                           $params, $this->is_tvarchive(), true);
    
    
    $tmp = 'tmp.mp4'; echo "\n";

    $p1 = "-pass 1 $acoder";
    $p2 = "-pass 2 $acoder $meta";
    
    
    foreach ($tries as $trie)
    {
      $cmd = "$trie $p1 $tmp";

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
    
    
    
    if (($err=Util::cmdQT("$qtfaststart $tmp ".Util::esc($dest),$times3, 'CONTINUE'))){
      // eg: /details/CNBC_20120717_040000_The_Costco_Craze_Inside_the
      Util::cmdQT("/petabox/sw/lib/python/qtfaststart/bin/qtfaststart $tmp ".Util::esc($dest),$times3);
    }

    $this->copyOrigMeta(); // backfills in any current metadata information
  }
}
?>
