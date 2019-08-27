$! Example "SHOW SYSTEM" task for dntask
$!
$! Put this command procedure in your SYS$LOGIN directory and
$! call it from Linux using the command 
$!    dntask myhost::show_system
$!
$! For an interactive task see INTERACTIVE.COM
$!
$ if f$mode() .eqs. "NETWORK" then $ define/nolog sys$output sys$net
$!
$ show system
$ exit
