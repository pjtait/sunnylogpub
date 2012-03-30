#!/bin/sh

kill -0 `cat /var/run/sunnylog.pid` 2> /dev/null
if [ $? != 0 ]; then
	logger "Restarted sunnylogpub"
	/home/root/mylogger/sunnylogpubd /home/SunnyData/pvdata
fi
