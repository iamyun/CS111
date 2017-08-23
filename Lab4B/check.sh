./lab4b --period=2 --scale=C --log="LOGFILE" <<-EOF
SCALE=F
PERIOD=1
START
STOP
OFF
EOF
ret=$?
if [ $ret -ne 0 ]
then
	echo "RETURNS RC=$ret"
fi

if [ ! -s LOGFILE ]
then
	echo "did not create a log file"
	let errors+=1
else
	echo "./lab4b supports and logs all sensor commands"
	for c in SCALE=F PERIOD=1 START STOP OFF SHUTDOWN
	do
		grep $c LOGFILE > /dev/null
		if [ $? -ne 0 ]
		then
			echo "DID NOT LOG $c command"
		else
			echo "	$c: OK"
		fi
	done

	cat LOGFILE
	echo "Passed Check 1"
fi

./lab4b --period=5 --scale=C --log="LOGFILE" <<-EOF
SCALE=F
PERIOD=2
STOP
START
OFF
EOF
ret=$?
if [ $ret -ne 0 ]
then
	echo "RETURNS RC=$ret"
fi

if [ ! -s LOGFILE ]
then
	echo "did not create a log file"
	let errors+=1
else
	echo "./lab4b supports and logs all sensor commands"
	for c in SCALE=F PERIOD=2 START STOP OFF SHUTDOWN
	do
		grep $c LOGFILE > /dev/null
		if [ $? -ne 0 ]
		then
			echo "DID NOT LOG $c command"
		else
			echo "	$c: OK"
		fi
	done

	cat LOGFILE
	echo "Passed Check 2"
fi