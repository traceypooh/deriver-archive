<?
require_once('Module.inc');

class WebM extends Module
{
  public function version() 
  { return '$Revision: 48707 $ $Date: 2013-01-06 20:14:18 +0000 (Sun, 06 Jan 2013) $'; }
  
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

    $ffmpeg = configGetPetaboxPath('bin-ffmpeg').' -v 0';
    

    // first, print clip info to log (since we'll throw out all ffmpeg output)
    if (!$identify)
      $identify = Module::identify($src);


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
                           "-acodec libvorbis -ab 128k -ac 2 -ar 44100",
                           $params, $this->is_tvarchive(), false);
    $cmd = $tries[0];
    
       
    $tmp = 'tmp.webm'; echo "\n";
    Util::cmdQT("$cmd  -pass 1 $acoder       $tmp", $times20);echo "\n";
    Util::cmdQT("$cmd  -pass 2 $acoder $meta $tmp", $times20);echo "\n";
    Util::cmdPP("mv $tmp ".Util::esc($dest));
  }
}
?>
