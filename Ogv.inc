<?
require_once('Module.inc');

class Ogv extends Module
{
  const ACODER = '-acodec libvorbis -ac 2 -ab 128k -ar 44100';


  public function version()
  { return '$Revision: 43652 $ $Date: 2012-05-07 06:48:41 +0000 (Mon, 07 May 2012) $'; }
  
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
      $identify = Shared::identify($src);


    // for any relevant metadata, make relevant cmd-line args to the cmd
    $times2 = Util::timeoutTime($identify,  '2x', '2h');
    $times10= Util::timeoutTime($identify, '10x','20h');

    
    $cmd = "$ffmpeg -y -i ".Util::esc($src)." -q:vscale 3 -b:v 512k -vcodec libtheora -pix_fmt yuv420p";

    list($cmd, $acoder) =
      Video::ffmpeg_params($cmd, $identify, $this->sourceFormat,
                           self::ACODER,
                           $params, $this->is_tvarchive());

    // metadata inserter
    $meta = Shared::videoMeta($this, array(
                                'title'         => '-metadata TITLE=',
                                'licenseurl'    => '-metadata LICENSE=',
                                'date'          => '-metadata DATE=',
                                'publisher'     => '-metadata ORGANIZATION=',
                                'director'      => '-metadata ARTIST=',
                                ), false).
      " -metadata LOCATION=http://archive.org/details/{$this->identifier} ";


    Util::cmdQT("$cmd -pass 1 -an     -f null /dev/null", $times10);
    Util::cmdQT("$cmd -pass 2 $acoder $meta $tmp"       , $times10);


    // This item /details/Orphanworksandmassdigitization20120412  killed this
    // technique (bad length in (only!) final ogv).  This way also fails for
    // /details/ShaneBrennanSittingTourBreathe  to be able to create an .ogv
    if (false  &&  $identify['audio']!=='false')
    {
      // why *only* put the $meta for videos with audio?
      // because for video-track-only oggs, libtheora makes the first
      // frame *not* a keyframe and that makes some players/browsers sad.
      //   eg: /details/DIA40   /details/commute
      $meta = Shared::videoMeta($this, array(
                                  'title'            => 'TITLE=',
                                  'director'         => 'ARTIST=',
                                  'licenseurl'       => 'LICENSE=',
                                  'date'             => 'DATE=',
                                  'publisher'        => 'ORGANIZATION='),
                                false).
        " LOCATION=http://archive.org/details/{$this->identifier} ";

      $vfi = "video.ogg";
      $afi = "audio.ogg";
      $afiC= "audioC.ogg";
      Util::cmdQT("$ffmpeg -i $tmp -vn -acodec copy $afi",$times2);
      Util::cmdQT("$ffmpeg -i $tmp -an -vcodec copy $vfi",$times2);

      $oggzcomment= configGetValue('bin-oggz-comment');
      $oggzmerge  = configGetValue('bin-oggz-merge');
      Util::cmdQT("$oggzcomment $afi -o $afiC $meta", $times2);
      Util::cmdQT("$oggzmerge $vfi $afiC -o tmp2.ogv", $times2);
      Util::cmd("rm -f $tmp $vfi $afi $afiC"); //cleanup
      $tmp = 'tmp2.ogv';
    }


    // backfills in any current metadata information
    $this->copyOrigMeta();
    
    // move derived file to final location
    Util::cmdPP("mv $tmp ".Util::esc($dest));
  }
}
?>