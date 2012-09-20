<?
require_once('Module.inc');

class Thumbnails extends Module
{
  // make sure we don't make TOO MANY thumbs/small files for given item!
  public $maxThumbsPerItem = 300;
  
  
  public function version()
  { return '$Revision: 45797 $ $Date: 2012-08-14 22:25:21 +0000 (Tue, 14 Aug 2012) $'; }
  
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

    chdir($this->tmp);

    $outDir   = rtrim($this->tmp, '/'); // kill trail "/" char
    $baseDir  = dirname($dest);
    $baseName = preg_replace('/000001.*$/', '', $dest);

    // make sure we don't make TOO MANY thumbs/small files for an entire item!
    if (!(is_integer($this->parameter)  &&  $this->parameter > 0))
      fatal("bad parameter passed in");

    if ($this->is_tvarchive())
    {
      $maxThumbs = 0;
    }
    else
    {
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
                                $maxThumbs, null, basename($baseName));
    if (!$thumbnails)
      return; // couldn't make thumbs for this movie.  move on to next module!
    
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
      $this->extraTarget($this->shortName("$baseDir/$thumb"), 'Thumbnail');
  }
}
?>
