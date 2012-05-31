<?
require_once('Module.inc');

class Thumbnails extends Module
{
  // make sure we don't make TOO MANY thumbs/small files for given item!
  public $maxThumbsPerItem = 300;
  
  
  public function version()
  { return '$Revision: 44162 $ $Date: 2012-05-27 04:47:39 +0000 (Sun, 27 May 2012) $'; }
  
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
    Util::cmdPP('rm -rf '.Util::esc($baseName).'[0-9]*.jpg');//remove any prior thumbnails!
    echo count($thumbnails)." thumbnails\n";

    chdir($this->tmp);

    $cmd = "mv ";
    foreach ($thumbnails as $thumb)
      $cmd .= Util::esc($thumb)." ";
    Util::cmdPP("$cmd ".Util::esc("$baseDir/"));

    foreach ($thumbnails as $thumbFile)
      $this->extraTarget($this->shortName("$baseDir/$thumbFile"), 'Thumbnail');
  }
}
?>
