#!/bin/sh

bin="farserver"
log="fs.log"

start_program() {
	path=`pwd`

	binfile=$path"/"$bin
	logfile=$path"/"$log

	nohup $binfile > $logfile &
}

stop_program() {
	killall -9 $bin > /dev/null
}

if [ $# != 1 ]; then
	echo "Usage: $0 start/stop"
	exit 1;
fi

case "$1" in
	start)
		stop_program
		start_program
	;;

	stop)
		stop_program
	;;
esac

