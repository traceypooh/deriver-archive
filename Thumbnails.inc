<?
require_once('Module.inc');

class Thumbnails extends Module
{
  // make sure we don't make TOO MANY thumbs/small files for given item!
  public $maxThumbsPerItem = 300;


  public function version()
  { return '$Id: d6e7ac944f06ca6e2b16aafcc0fec7d60cff6693 $'; }


  public function derive($src=null, $dest=null, $identify=null, $first=true, $title=1)
  {
    if ($src==null)
      return $this->deriveVideo(); // calls self w/ args set, ISO Image preprocessing if needed


    chdir($this->tmp);

    $outDir   = rtrim($this->tmp, '/'); // kill trail "/" char
    $baseDir  = dirname($dest);
    $baseName = preg_replace('/00000[01]\.jpg$/', '', $dest);


    if (is_integer($this->parameter)){
      // make sure we don't make TOO MANY thumbs/small files for an entire item!
      if (!(is_integer($this->parameter)  &&  $this->parameter > 0))
        fatal("bad parameter passed in");

      if ($this->is_tvarchive()){
        $maxThumbs = 0;
      }
      else{
        // scale down the number of thumbs/file to the number of files
        // so the overall total of "maxThumbsPerItem" remains constant
        $maxThumbs = floor($this->maxThumbsPerItem / $this->parameter);
        if ($maxThumbs <= 1)
          $maxThumbs = 2; // make at least two thumbs per movie
      }

      echo "    Thumbnail maker basename: $baseName\n";
      echo "    Thumbnail maker base dir: $baseDir\n";
      if ($maxThumbs)
        echo "    NOTE: There are {$this->parameter} source files in item deriving Thumbnails (derive up to $maxThumbs thumbnails now)\n";


      // make the thumbnails
      $thumbnails = Video::frames($src, $outDir, $this->is_tvarchive(),
                                  $maxThumbs, $identify, basename($baseName));
    }
    else if (preg_match('/^ffmpeg(.*)-i - (.*)$/', $this->parameter, $mat)  &&  $mat[1][0]===' '){
      $cmd = "ffmpeg {$mat[1]} -i ".Util::esc($src)." {$mat[2]}";

      chdir($this->tmp);
      Util::cmdPP($cmd);
      $thumbnails = glob("*.jpg");
      if (!count($thumbnails))
        return; // no thumbs made!

      // we need to ensure the "dest" gets made -- have it be our 1st thumb
      $jpg = array_shift($thumbnails);
      Util::cmd("mv ".Util::esc($jpg)." ".Util::esc(basename($dest)), "PRINT");

      // as of now, shouldn't be any more thumbs, but if there were, add them
      $pre = preg_replace('/_$/','', basename($baseName));
      foreach ($thumbnails as $jpg)
        Util::cmd("mv ".Util::esc($jpg)." ".Util::esc($pre.$jpg), "PRINT");
      $thumbnails = glob("*.jpg");
    }
    else {
      fatal("unexpected logic");
    }
    if (!$thumbnails)
      return; // couldn't make thumbs for this video.  move on to next module!

    Util::cmdPP('mkdir -p '.Util::esc($baseDir));
    Util::cmdPP('rm -rf '.Util::esc($baseName).'[0-9]*.jpg');//remove any prior thumbnails! (think this is actually noop/broken!  tracey aug2012)
    echo count($thumbnails)." thumbnails\n";

    chdir($this->tmp);

    // make sure glob and list of $thumbnails is identical!!
    $check = glob("*_[0-9][0-9][0-9][0-9][0-9][0-9].jpg");
    if ((($diff = array_diff($check, $thumbnails))  &&  count($diff))  ||
        (($diff = array_diff($thumbnails, $check))  &&  count($diff))){
      StatsD::increment('thumbnails.badframes');
      fatal("logic error: ".print_r($diff,1));
    }

    // glob for pathological long/broken items (esp. TV!) that wind up with 5000 thumbs.
    // Also makes us not exceed cmd-line max len.
    // Also makes for quieter catalog task logs. ;-)
    Util::cmdPP("mv *_[0-9][0-9][0-9][0-9][0-9][0-9].jpg ".Util::esc("$baseDir/"));

    foreach ($thumbnails as $thumb)
      $this->extraTarget($this->shortName("$baseDir/$thumb"), $this->targetFormat);
  }
}
