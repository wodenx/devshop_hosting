<?php


/**
 * Menu callback that is invoked by GitHub WebHook facility to create
 * a code pull task.
 *
 * @TODO: Save "Last Pull" and "Last Pull Status" when git pull task is complete.
 */
function devshop_pull_callback($project, $hash) {
  
  // Get the access from list
  $acl = explode("\n", trim(variable_get('devshop_pull_ip_acl', '')));

  // I wish I didn't have to do this
  for($i = 0; $i < count($acl); $i++) {
    $acl[$i] = trim($acl[$i]);
  }

  // Make sure the client's IP address is on the list
  if (!in_array(ip_address(), $acl)) {
    $message = ip_address() . " is not authorized to invoke a Pull Code request.";
    watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_ERROR);
    print "$message<br>";
    return;
  }

  $message = "GitHub post-receive hook invoked by " . ip_address();
  watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_INFO);
  print "$message<br>";


  if (strlen($project) < 1 || strlen($hash) != 32) {
    $message = "Invalid/missing parameters in URL!";
    watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_ERROR);
    print "$message<br>";
    return;
  }

  // Based on the project name, get the node ID for the project node.
  // Is need remove the prefix for find the nid.
  $pnid = db_result(db_query(
    "SELECT nid FROM {hosting_context} WHERE name = '%s'", str_replace('project_', '', $project)));

  // Load the entire node
  if (!$pnode = node_load($pnid)) {
    $message = "Unable to load project node!";
    watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_ERROR);
    print "$message<br>";
    return;
  }

  // Make sure the security code is valid
  if (_devshop_pull_hash_create($pnode) != $hash) {
    $message = "Security code $hash is not valid!";
    watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_ERROR);
    print "$message<br>";
    return;
  }
  
  // Make sure the platform has pull callback enabled
  if ($pnode->pull_method != DEVSHOP_PULL_CALLBACK){
    $message = "Project is NOT configured to use Pull Code URL callback!";
    watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_ERROR);
    print "$message<br>";
    return;
  }

  //Search platforms with pull enable of this project
  $platforms = $pnode->project_objects['platform']; 

  //Put timestamp
  db_query('UPDATE {hosting_devshop_pull_projects} SET last_pull = %d, last_pull_status = %d WHERE project_nid = %d', time(), 0, $pnid);

  foreach ($platforms as $platform_nid => $site) {
    //If pull enabled then add to task
    if (db_result(db_query("SELECT pull_enabled FROM {hosting_devshop_pull_platforms} WHERE platform_nid=%d", $site_nid))) {
      $message = "Queuing a pull task for node id $platform_nid";
      watchdog(DSGH_WD_TYPE, $message, array(), WATCHDOG_INFO);
      print "$message<br>";
      devshop_pull_task($platform_nid);
    }
  }
}

/**
 * Create the full URL that is displayed in the project node view
 * and given to the GitHub WebHook to invoke a pull after a commit.
 */
function _devshop_pull_callback_url($node) {
  return url(DEVSHOP_PULL_CALLBACK_URL .
	'/' . $node->hosting_name .
	'/' . _devshop_pull_hash_create($node), 
	array('absolute' => TRUE));
}

/**
 * Create a security hash code based on the platform node
 */
function _devshop_pull_hash_create($node) {
  return md5($node->hosting_name . $node->nid);
}