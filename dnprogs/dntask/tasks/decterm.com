$! Example "DECNET" task for dntask
$!
$! Put this command procedure in your SYS$LOGIN directory and
$! call it from Linux using the command 
$!    dntask myhost::decterm
$!
$ if f$mode() .eqs. "NETWORK" then $ define/nolog sys$output sys$net
$!
$ remnode=f$element(0, ":", "''f$trnlnm("sys$rem_node")'")
$ set display/create/node='remnode'
$ create/term/detach
$!
$! NOTE: This write is essential to the DECnet protocol.
$!
$ write sys$output "DECterm started on ''remnode'"
$ exit
