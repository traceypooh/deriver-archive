<?
require_once('Module.inc');

class Ogv extends Module
{
  const ACODER = '-acodec libvorbis -ac 2 -ab 128k -ar 44100';


  public function version()
  { return '$Revision: 44611 $ $Date: 2012-06-14 19:27:19 +0000 (Thu, 14 Jun 2012) $'; }
  
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

    $ffmpeg     = configGetPetaboxPath('bin-ffmpeg').' -v 0';

    $tmp = 'tmp.ogv';
    $params="400x300";

    
    // first, print clip info to log (since we'll throw out all ffmpeg output)
    if (!$identify)
      $identify = Module::identify($src);


    // for any relevant metadata, make relevant cmd-line args to the cmd
    $times2 = Util::timeoutTime($identify,  '2x', '2h');
    $times10= Util::timeoutTime($identify, '10x','20h');

    
    $cmd = "$ffmpeg -y -i ".Util::esc($src)." -q:vscale 3 -b:v 512k -vcodec libtheora -pix_fmt yuv420p";

    list($cmd, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat,
                           self::ACODER,
                           $params, $this->is_tvarchive());

    // metadata inserter
    $meta = Video::videoMeta($this, array(
                                'title'         => '-metadata TITLE=',
                                'licenseurl'    => '-metadata LICENSE=',
                                'date'          => '-metadata DATE=',
                                'publisher'     => '-metadata ORGANIZATION=',
                                'director'      => '-metadata ARTIST=',
                                ), false).
      " -metadata LOCATION=http://archive.org/details/{$this->identifier} ";


    Util::cmdQT("$cmd -pass 1 -an     -f null /dev/null", $times10);
    Util::cmdQT("$cmd -pass 2 $acoder $meta $tmp"       , $times10);


    // backfills in any current metadata information
    $this->copyOrigMeta();
    
    // move derived file to final location
    Util::cmdPP("mv $tmp ".Util::esc($dest));
  }
}
?>