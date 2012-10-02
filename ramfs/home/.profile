if [ "$(id -u)" == "0" ] ; then
	PS1='\[\033[01;31m\]hackme\[\033[01;34m\] \W #\[\033[00m\] '
else
	PS1='\[\033[01;32m\]not-yet-root@hackme\[\033[01;34m\] \w \$\[\033[00m\] '
fi
