<?
abstract class Module
{
  public function __construct($src, $id, $suffix)
  {
    $cname = get_class($this);
    echo "\n\n\n======================= STARTING $cname ($src => $src.$suffix) =======================\n\n\n";

    $this->sourceFile=$src;
    $this->identifier = $id;
    $this->sourceFormat="QuickTime";
    $this->tmp='/tmp/deriver';
    passthru("rm -rfv $this->tmp");
    mkdir($this->tmp);
    $this->targetFile="$src.$suffix";

    if ($cname=='Thumbnails')
    {
      $base = basename($src);
      $this->targetFile=dirname($src)."/{$base}.thumbs/{$base}_000001.{$suffix}";
      $this->parameter = 1;
    }
    else
    {
      // see if there is a "parameter" value we should setup
      $x=simplexml_load_file("{$_SERVER['PETABOX_HOME']}/derivations.xml");
      $nodes = $x->xpath("/formats/format[class='$cname']/variations/variation[@parameter!='']/@parameter");
      if ($nodes  &&  count($nodes))
        $this->parameter = ((string)$nodes[0]->parameter[0]);
    }
    
    chdir($this->tmp);
    try { $this->derive(); } catch (Exception $e){}
    passthru("rm -rfv $this->tmp");
  }

  protected function copyOrigMeta(){}
  protected function extraTarget(){}
  protected function shortName(){}
  protected function is_tvarchive(){ return false; }
  protected function identify($file){
    $identify = Video::identify($file,1);
    print_r($identify);
    return $identify;
  }

  abstract public function derive();

}
