$! Example "INTERACTIVE" task for dntask
$!
$! Put this command procedure in your SYS$LOGIN directory and
$! call it from Linux using the command 
$!    dntask -i myhost::interactive
$! You can then type in DCL commands and the output will be send to
$! standard output on the Linux machine.
$!
$! For a non-interactive task see SHOW_SYSTEM.COM
$!
$ open/read/write linux sys$net
$ nextcmd:
$!
$ cmd="!"
$ read /prompt="" /end=eof_end /error=error_end linux cmd
$ if "''cmd'" .eqs. "exit" then $ goto endtask
$ if "''cmd'" .eqs. "" then $ goto nextcmd
$ define/nolog/user sys$output linux
$ define/nolog/user sys$input linux
$ cmd
$ goto nextcmd
$!
$ error_end:
$ eof_end:
$ endtask:
$ close/nolog linux
$ exit
