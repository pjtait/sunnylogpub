#!/bin/sh

appdir=`dirname $0`
kill -0 `cat /var/run/sunnylog.pid` 2> /dev/null
if [ $? != 0 ]; then
	logger "Restarted sunnylogpub"
	${appdir}/sunnylogpubd $1
fi
