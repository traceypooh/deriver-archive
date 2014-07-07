<?
require_once('Module.inc');

class Ogv extends Module
{
  public function version()
  { return '$Revision: 59932 $ $Date: 2014-06-08 00:40:43 +0000 (Sun, 08 Jun 2014) $'; }
  


  public function derive($src=null, $dest=null, $identify=null, $first=true, $title=1)
  {
    if ($src==null)
      return $this->deriveVideo(); // calls self w/ args set, ISO Image preprocessing if needed


    chdir($this->tmp)  ||  fatal("unable to change to temp dir");

    $ffmpeg     = Video::ffmpegV0();

    $tmp = 'tmp.ogv';
    $params="400x300 {$this->parameter}";

    

    // for any relevant metadata, make relevant cmd-line args to the cmd
    $times2 = Util::timeoutTime($identify,  '2x', '2h');
    $times10= Util::timeoutTime($identify, '10x','20h');

    
    $cmd = "$ffmpeg -y -i ".Util::esc($src)." -q:vscale 3 -b:v 512k -vcodec libtheora -pix_fmt yuv420p";

    list($tries, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat, '', $params, $this->is_tvarchive());
    $cmd=$tries[0];

    // metadata inserter
    $meta = Video::videoMeta($this, array(
                                'title'         => '-metadata TITLE=',
                                'licenseurl'    => '-metadata LICENSE=',
                                'date'          => '-metadata DATE=',
                                'publisher'     => '-metadata ORGANIZATION=',
                                'director'      => '-metadata ARTIST=',
                                ), false).
      " -metadata LOCATION=https://archive.org/details/{$this->identifier} ";


    Util::cmdQT("$cmd -pass 1 $acoder -f null /dev/null", $times10);
    Util::cmdQT("$cmd -pass 2 $acoder $meta $tmp"       , $times10);


    // backfills in any current metadata information
    $this->copyOrigMeta();
    
    // move derived file to final location
    Util::cmdPP("mv $tmp ".Util::esc($dest));
  }
}
?>