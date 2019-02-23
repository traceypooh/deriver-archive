<?

// Class to allow methods to use Named Parameters (as close as PHP can get).
// Leveraging the "kwargs" pattern in python, a method using this can be invoked with
// _either_ positional _or_ named parameters.

/*
// Let's assume this example:
function print_money($modest = false, $color = 'green') {
  extract(Arg::destructure(), EXTR_REFS);
  ..
}

// These are then equivalent:
print_money(true, 'silver');
print_money(['color' => 'silver', 'modest' => true]);

// Params not set will get defaults.  So inside "print_money()", $modest will be false
// (and $color will be 'silver') for invocation:
print_money(['color' => 'silver']);


// You can alternatively use a second argument as an object to set instance vars into
class Mint {
  function print_money($modest = false, $color = 'green') {
    Arg::destructure($modest, $this);
  }
}

$m = new Mint();
$m->print_money(['color' => 'silver']);

//  $m->modest === false
//  $m->color  === 'silver'

TBD:
  - what if expected and/or default value for 1st arg is hashmap?  disallow/fatal?
  - what if pass in _both_ hashmap 1st arg _and_ later positionals?  (later are ignored right now)
*/

class Arg {
  /**
   * Determines if the invoking method is being used in a way where (typically one) parameter
   * is actually a hashmap representing named parameters to 'destructure' into local variables
   * like they were called positionally.
   *
   * @param $args  can pass in an explicit argument to test/destructure it -- else the _first_
   *               parameter of the method is assumed
   * @param $set_this  object you'd like all returned key/vals to be automatically set as instance vars
   * @return hashmap of key/vals the caller should call 'extract(.., EXTR_REFS)' on.  returns []
   *         when caller is invoking method positionally and/or there are no overrides
   **/
  public static function destructure($map = null, $set_this = false) {
    if ($map !== null  &&  !self::is_assoc($map))
      return []; // arg to test isnt a hashmap -- method in question is being called positionally

    // use the stack trace to get a hold of the class/method _and_ the args passed in
    $trace = current(array_reverse(debug_backtrace(0, 2)));
    // printr($trace);

    if ($map === null) {
      // we assume that (only) the 1st argument to the method was passed in as a hashmap and all
      // other arguments were omitted.  fetch the hashmap.
      $map = $trace['args'][0];
      if (!self::is_assoc($map))
        return []; // wasn't a hashmap after all -- is being called positionally
    }

    if (isset($trace['class'])) {
      $ref = new ReflectionMethod($trace['class'], $trace['function']);
    } else {
      $ref = new ReflectionFunction($trace['function']);
    }

    // now get its constructor parameter names and default values, and override any value as desired
    $extracts = [];
    foreach ($ref->getParameters() as $param) {
      //if (!$param->getPosition()  &&  is_array($param->getDefaultValue()))
      //  fatal('first parameter cant be an array');

      $param_name = $param->getName();
      if (isset($map[$param_name])) {
        $extracts[$param_name] = &$map[$param_name];
        if ($set_this)
          $set_this->$param_name = &$map[$param_name];
      } else {
        // NOTE: we do it this way in case param default value _is a reference_
        $defaultVal = $param->getDefaultValue();
        if ($param->isPassedByReference()) {
          $extracts[$param_name] = &$defaultVal;
          if ($set_this)
            $set_this->$param_name = &$defaultVal;
          unset($defaultVal);
        } else {
          $extracts[$param_name] = $defaultVal;
          if ($set_this)
            $set_this->$param_name = $defaultVal;
        }
      }
    }

    // now add back in any potential "bonus key/vals" that caller may have in $map that arent
    // explicitly in method's parameter list.
    foreach ($map as $k => $v) {
      if (!isset($extracts[$k])) {
        $extracts[$k] = &$v;
        if ($set_this)
          $set_this->$k = &$v;
        unset($v);
      }
    }
    //printr($extracts);

    // caller can now run: extract($extracts, EXTR_REFS)
    // on this return value and it's just like all arguments were sent in positionally!
    return $extracts;
  }


  /**
   * Returns whether or not passed in argument seems to be a associative array / hashmap or not.
   * NOTE: This can be "fooled" by a "sparse array" or an array starting with 1, etc.
   * @param $arg to test
   * @return boolean truthy/falsey
   */
  private static function is_assoc($arg) {
    // Once we've determined it's either a flat array _or_ a hashmap, try to compare the keys
    // to see if they're all numeric _and_ sequential starting at 0.  if so, assume it's an array.
    return ($arg !== null  &&  is_array($arg)  &&  array_diff_key($arg, array_keys(array_keys($arg))));
  }
}
