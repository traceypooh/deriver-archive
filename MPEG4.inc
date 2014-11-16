<?
require_once('Module.inc');

class MPEG4 extends Module
{
  const ACODER = '-acodec aac -strict experimental';
  
  
  public function version()
  { return '$Revision: 62875 $ $Date: 2014-10-30 21:22:32 +0000 (Thu, 30 Oct 2014) $'; }

  
  public function derive($src=null, $dest=null, $identify=null, $first=true, $title=1)
  {
    if ($src==null)
      return $this->deriveVideo(); // calls self w/ args set, ISO Image preprocessing if needed

    
    chdir($this->tmp)  ||  fatal("unable to change to temp dir");

    $ffmpeg             = Video::ffmpegV0($src);

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

    // if we are *making* a 720P derivative...
    $HD = (strpos($params,'x720')!==false);
    if ($HD){
      // ... then verify (for now, for certain collections) that the *source* is at least HD...
      $mxml = json_decode(json_encode(simplexml_load_file($this->itemDir . $this->identifier . "_meta.xml")),1);
      if (!is_array($mxml['collection']))
        $mxml['collection']=array($mxml['collection']);
      
      if (in_array('internetarchivepresents', $mxml['collection'])  &&  !FormatGetter::isHD($identify['width'], $identify['height'])){
        echo "source video is only {$identify['height']}P -- skipping this 720P derivative\n";
        return;
      }
    }


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
    

    Util::cmdQT("mv $tmp ".Util::esc($dest));

    $this->copyOrigMeta(); // backfills in any current metadata information
  }
}
