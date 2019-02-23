<?

// NOTE: config -- may want to tweak these to taste:
date_default_timezone_set('America/Los_Angeles');
define('DEBUG', 0);
// NOTE: config -- may want to tweak these to taste:

errtesting();


class Config {
  public static function value($key) {
    return (strncmp('bin-', $key, 4)==0 ? substr($key, 4) : $key);
  }

  public static function petaboxPath($val) {
    return Config::value($val);
  }
}

// stubbed out
trait Assert {}
class TV {}


function printr($obj) {
  print_r($obj);
}

// Prints the (debugging) message if we are in debug mode.
function debug($message) {
  if (isset($_REQUEST['cacheDebug'])  ||  DEBUG) {
    echo '[' . date('h:i:s') . "] $message" . (php_sapi_name()=='cli' ? '' : '<br/>') . "\n";
    flush();
  }
}

function msg($msg) {
  error_log($msg);
}

// Fatals script (non-0 exit status) with the passed in msg.
// Will throw an exception instead if special global is set.
function fatal($msg) {
  if (isset($GLOBALS['fatal-exceptions']))
    throw new Exception($msg); // typically used to protect pages that ONLY output XML

  error_log("FATAL ERROR: $msg");
  flush();
  exit(1);
}


// like errstrict() but LOGS WARNINGS AND ERORRS TO WEB PAGE (and log 8-)
// it also will suppress certain overly excessive strict warnings
// (like associative array key checking)
function errtesting() {
  ini_set('display_errors', 'On');
  ini_set('log_errors', 'On');
  error_reporting(E_STRICT | E_ALL);
  set_error_handler('myErrorHandler');
}


// error handler function (used only in PHP5)
function myErrorHandler($errno, $errstr, $errfile, $errline) {
  $debug = "$errstr [".$errfile.":$errline]";

  $fat=false;
  switch ($errno) {
    case E_USER_ERROR:
      echo "<b>FATAL</b> [$errno] $errstr in line $errline of file $errfile";
      $fat=true;
      break;
    case E_USER_WARNING:
      echo "<b>ERROR</b> [$errno] $errstr";
      break;
    case E_USER_NOTICE:
      echo "<b>WARNING</b> [$errno] $errstr";
      break;
    default:
      if (preg_match('/Undefined index: /',                $errstr) ||
          preg_match('/Undefined offset: /',               $errstr) ||
          preg_match('/Offset not contained in string\./', $errstr)) {
        return;
      }
      echo "Unknown err: $debug\n";
      break;
  }

  error_log($debug);
  if ($fat)
    exit(1);
}


// Autoloader!  Loads missing class definitions dynamically
spl_autoload_register(function ($class_name) {
  require_once("$class_name.inc");
});
