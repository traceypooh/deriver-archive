<?
require_once('Module.inc');

// This is a very specific broadcast playout server related MPEG2 file that
// was asked for by our PEG partner, Denver Open Media.
//
// It is only done on a per-collection "opt in" basis.

class MPEG2 extends Module
{
  public function version() 
  { return '$Revision: 44080 $ $Date: 2012-05-22 19:17:21 +0000 (Tue, 22 May 2012) $'; }
  
  public function derive()
  {
    $src = $this->sourceFile;
    $dest= $this->targetFile;
    $tmp = $this->tmp.'tmp.mpg';
    $copy= false;

    $params = $this->parameter;
    $ffmpeg =  configGetPetaboxPath('bin-ffmpeg').' -v 0';

    $identify = Shared::identify($src);
    if (!$identify['width']  ||  !$identify['height'])
      fatal("Video width/height not calculable -- likely no video track!");


    // john hauser especially wanted the rect pixel 720x480 instead of
    // the square pixel 640x480 we'd be making...
    list($vf) = Video::vf("-s 640x480", $identify, false, true);
    // ...so rescale once more!
    // (and remove our normal deinterlace step since these are intended
    //  for TV interlaced broadcast anyway)
    $vf = Video::yadif_nix($vf) .  ',scale=720:480  -aspect 4:3 ';
    


    if ($identify['length'] > 12*3600)
    {
      // this item came in as over 1-day long and low bitrate (obviously wrong!)
      //   /details/dom-392037-theworldtomorrowthewhitehorse
      // so streamcopy to a tmp file to "launder" the A/V streams as a workaround
      $copy = $this->tmp.'copy.mpg';
      echo "\n\nNOTE: this video had a implausibly long length -- streamcopy to temp dest to try to \"launder\" it\n\n";
      Util::cmdQT("$ffmpeg -i ".Util::esc($src)." -acodec copy -vcodec copy $copy", 4);
      $identify = Shared::identify($copy);
    }

    $audio = ($identify['audio']  &&  $identify['audio'] != 'false');
        
    $times6 = Util::timeoutTime($identify, '6x', '8h');

    // NOTE: this makes a MPEG-PS format target file, and the *format* does *NOT*
    // provide/allow for comments/metadata.  So the $meta stuff below
    // is useless, but harmless, unfortunately.  I've left it here in case
    // in the future we switch to MPEG-TS (allows "title" at least!) and/or
    // it somehow starts magically working w/ ffmpeg updates or otherwise...

    // for inserting metadata
    $meta = Shared::videoMeta($this, array(
                                'title'         => '-metadata title=',
                                'date'          => '-metadata year=',
                                'director'      => '-metadata author=',
                                'licenseurl'    => '-metadata comment=license:',
                                ), true);
    
    Util::cmdQT("$ffmpeg -i ".($copy ? $copy : Util::esc($src))." ".($audio ? "" : "-an")." $params $vf $meta $tmp", $times6);
    


    // backfills in any current metadata information
    $this->copyOrigMeta();

    Util::cmdPP("mv $tmp ".Util::esc($dest));      
    
    if ($this->sourceFormat == 'MPEG2')
    {
      $shortsrc = $this->filesXML->baseNameInItem($src);
      $shortdest= $this->filesXML->baseNameInItem($dest);
      echo "\n\n NOTE: both source and dest are MPEG2!\n";
      echo " Effectively replacing $shortsrc with $shortdest!\n";
      
      if (!($this->filesXML->renameFile($shortsrc, $shortdest)))
        fatal("rename in files.xml failed!");

      $this->filesXML->write();
      unlink($src);
      $this->noTargets();
      return;
    }
  }
}
?>
