#!/bin/bash -x
#
# Runs filebench alongside Duet microbenchmarks, and dumps raw numbers in an R
# file. No results are kept for filebench.
# Supported workloads: "varmail", "webserver", "webproxy", "fileserver"
#
# @1: partition to run filebench on
#

# Set up environment variables
cd "$(dirname "$0")"
basedir="$(pwd)"
fspath="$basedir/fbench"
outdir="$basedir/results/"
cgout="$basedir/cgrabber.out"
fbprof="$basedir/filebench.prof"

# Program paths
dummyp="$basedir/../dummy_task/dummy"
cgrabp="$basedir/cpugrabber/cpugrabber"
pgrabp="$basedir/cpugrabber/process-grabber"

workload="webserver"
fetchfreq=(0 10 20 40)

# Set up argument variables
fspart="$1"
fssize="`sudo fdisk -l /dev/sdb1 | head -1 | awk '{print $5}'`"

# build_wkld.sh variables, no need to touch
explen=8000
profgran=20
source build_wkld.sh

# Runs one configuration of the microbenchmark
# @1: configuration -- duet off (0), duet on (1), duet register (2)
# @2: fetch frequency in ms (if @1 == 2)
# @3: duet item type -- events (0), states (1)
# @4: experiment suffix for output
run_one () {
	config=$1
	ffreq=$2
	evtbased=$3
	expsfx=$4

	duet status stop
	if [ $config -ge 1 ]; then
		echo -ne "- Starting the duet framework... " | tee -a $logpath
		duet status start
		echo -ne "Done.\n" | tee -a $logpath
	fi

	dargs="-d 300000"
	if [ $evtbased -eq 1 ]; then
		dargs="$dargs -e"
	fi
	if [ $config -eq 2 ]; then
		dargs="$dargs -o"
		if [ $ffreq -gt 0 ]; then
			dargs="$dargs -f $ffreq"
		fi
	fi

	for expiter in $(seq 1 $numreps); do
		# Start the cpugrabber before we start the microbenchmark
		sudo $cpugrabberp -r 5000 -t 300000 2> $cgout &
		cgid=$!

		# Start dummy task, and wait until it's done
		sudo $dummyp $dargs
		wait $cgid

		# Log cpugrabber results
		if [ $expiter -eq 1 ]; then
			cgresults="`sudo $psgrabberp dummy $cgout`"
		else
			cgresults="$cgresults, `sudo $psgrabberp dummy $cgout`"
		fi
	done

	# Append results in R file
	echo -ne "Exporting collected stats... " | tee -a $logpath
	echo -e "${expsfx} <- c($cgresults);\n" >> $rfpath
	echo -ne "Done.\n" | tee -a $logpath
}

# Starts filebench and runs each iteration of the microbenchmark
run_experiments () {
	echo -ne "- Removing all files from '$fspath'..." | tee -a $logpath
	if [[ $fspath == /media/* ]]; then
		rm -Rf $fspath/*
	else
		echo "\nError: $fspath not under /media." | tee -a $logpath
		exit 1
	fi
	echo " Done." | tee -a $logpath

	echo -ne "- Compiling filebench profile..." | tee -a $logpath
	compileprof $wkld
	echo " Done." | tee -a $logpath

	echo -ne "- Clearing buffer cache... " | tee -a $logpath
	sudo sh -c "sync && echo 3 > /proc/sys/vm/drop_caches"
	echo -ne "Done.\n" | tee -a $logpath

	# Start filebench and count times we've seen "Running..." sequence
	running=0
	echo -e "- Starting filebench... " | tee -a $logpath
	sudo filebench -f $fbprof 2>&1 | grep --line-buffered "Running..." | \
	while read; do
		if [ $running == 0 ]; then
			running=1
		elif [ $running == 1 ]; then
			running=2

			echo -e "- Running microbenchmarks..." | tee -a $logpath
			echo -e "  >> Duet off" | tee -a $logpath
			echo -e "# Results when Duet is off" | tee -a $rfpath
			run_one 0 0 0 "x0"

			echo -e "  >> Duet state-based" | tee -a $logpath
			echo -e "# Results when state-based Duet is on" | tee -a $rfpath
			run_one 1 0 0 "s0"

			for ffreq in ${fetchfreq[@]}; do
				echo -e "  >> Duet state-based, fetch every ${ffreq}ms" | tee -a $logpath
				echo -e "# Results when state-based Duet is fetching every ${ffreq}ms" | tee -a $rfpath
				run_one 2 $ffreq 0 "s$ffreq"
			done

			echo -e "  >> Duet event-based" | tee -a $logpath
			echo -e "# Results when event-based Duet is on" | tee -a $rfpath
			run_one 1 0 1 "e0"

			for ffreq in ${fetchfreq[@]}; do
				echo -e "  >> Duet event-based, fetch every ${ffreq}ms" | tee -a $logpath
				echo -e "# Results when event-based Duet is fetching every ${ffreq}ms" | tee -a $rfpath
				run_one 2 $ffreq 1 "e$ffreq"
			done
		elif [ $running == 2 ]; then
			# If we got here, we're done. Filebench must die.
			fbpid="`ps aux | grep "sudo filebench" | grep -v grep \
				| awk '{print $2}'`"
			if [[ ! -z $fbpid  ]]; then
				kill -INT $fbpid
			fi
		fi
	done

	# Keep syslog output in $output.log
	echo "- Appending syslog to $outpfx.syslog" | tee -a $logpath
	cp /var/log/syslog $outpfx.syslog
}

# Initialize some environment variables
case $workload in
"webserver")	wkld="wsv"	;;
"varmail")		wkld="var"	;;
"webproxy")		wkld="wpx"	;;
"fileserver")	wkld="fsv"	;;
*)
	echo "I don't recognize workload '$workload'. Goodbye."
	exit 1
	;;
esac

datstr="$(date +%y%m%d-%H%M)"		# Date string
outpfx="$outdir${wkld}_${datstr}"	# Output file prefix

logpath="${outpfx}.log"				# Log file path
rfpath="${outpfx}.R"				# R file path

# Create filesystem and mount it
sudo umount $fspart
sudo mkdir -p $fspath
sudo logrotate -f /etc/logrotate.conf
echo "- Creating ext4 filesystem on $fspart..." | tee -a $logpath
sudo mkfs.ext4 -f $fspart
echo "- Mounting ext4 filesystem on $fspath..." | tee -a $logpath
sudo mount $fspart $fspath

# Do what filebench wants
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

echo -e "\n=== Starting experiments ===" | tee -a $logpath
run_experiments
echo -e "\n=== Evaluation complete (results in ${outdir}) ===" | tee -a $logpath

# Cleanup temporary files and unmount fs
rm $fbprof fbperson.f
echo "- Unmounting ext4 filesystem on $fspath..." | tee -a $logpath
sudo umount $fspath
