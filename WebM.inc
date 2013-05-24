<?
require_once('Module.inc');

class WebM extends Module
{
  public function version() 
  { return '$Revision: 51514 $ $Date: 2013-05-20 19:19:45 +0000 (Mon, 20 May 2013) $'; }

  
  public function derive($src=null, $dest=null, $identify=null, $first=true, $title=1)
  {
    if ($src==null)
      return $this->deriveVideo(); // calls self w/ args set, ISO Image preprocessing if needed


    chdir($this->tmp)  ||  fatal("unable to change to temp dir");

    $ffmpeg = Video::ffmpegV0();
    

    $times20 = Util::timeoutTime($identify, '20x', '30h');


    // metadata inserter
    $meta = Video::videoMeta($this, array(
                                'title'         => '-metadata title=',
                                'date'          => '-metadata year=',
                                'director'      => '-metadata author=',
                                'licenseurl'    => '-metadata comment=license:',
                                ), true);

    
    $params = str_replace(' /petabox/', " {$_SERVER['PETABOX_HOME']}/", $this->parameter);

    $cmd = ("$ffmpeg -y -i ".Util::esc($src)." -vcodec webm ");

    list($tries, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat,
                           "-acodec libvorbis -b:a 128k -ac 2 -ar 44100",
                           $params, $this->is_tvarchive(), false);
    $cmd = $tries[0];
    
       
    $tmp = 'tmp.webm'; echo "\n";
    Util::cmdQT("$cmd  -pass 1 $acoder       $tmp", $times20);echo "\n";
    Util::cmdQT("$cmd  -pass 2 $acoder $meta $tmp", $times20);echo "\n";
    Util::cmdPP("mv $tmp ".Util::esc($dest));
  }
}
?>
