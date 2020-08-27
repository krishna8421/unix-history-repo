#!/bin/ksh

# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright (c) 2017, 2020 by Delphix. All rights reserved.
#

#
# Description:
# Trigger fio runs using the random_readwrite_fixed job file. The number of runs and
# data collected is determined by the PERF_* variables. See do_fio_run for
# details about these variables.
#
# The files to read and write from are created prior to the first fio run,
# and used for all fio runs. The ARC is cleared with `zinject -a` prior to
# each run so reads will go to disk.
#

. $STF_SUITE/include/libtest.shlib
. $STF_SUITE/tests/perf/perf.shlib

function cleanup
{
	# kill fio and iostat
	pkill fio
	pkill iostat
	recreate_perf_pool
}

trap "log_fail \"Measure IO stats during random read write load\"" SIGTERM
log_onexit cleanup

recreate_perf_pool
populate_perf_filesystems

# Aim to fill the pool to 50% capacity while accounting for a 3x compressratio.
export TOTAL_SIZE=$(($(get_prop avail $PERFPOOL) * 3 / 2))

# Variables for use by fio.
if [[ -n $PERF_REGRESSION_WEEKLY ]]; then
	export PERF_RUNTIME=${PERF_RUNTIME:-$PERF_RUNTIME_WEEKLY}
	export PERF_RANDSEED=${PERF_RANDSEED:-'1234'}
	export PERF_COMPPERCENT=${PERF_COMPPERCENT:-'66'}
	export PERF_COMPCHUNK=${PERF_COMPCHUNK:-'4096'}
	export PERF_RUNTYPE=${PERF_RUNTYPE:-'weekly'}
	export PERF_NTHREADS=${PERF_NTHREADS:-'8 16 32 64'}
	export PERF_NTHREADS_PER_FS=${PERF_NTHREADS_PER_FS:-'0'}
	export PERF_SYNC_TYPES=${PERF_SYNC_TYPES:-'0 1'}
	export PERF_IOSIZES='8k 64k'
elif [[ -n $PERF_REGRESSION_NIGHTLY ]]; then
	export PERF_RUNTIME=${PERF_RUNTIME:-$PERF_RUNTIME_NIGHTLY}
	export PERF_RANDSEED=${PERF_RANDSEED:-'1234'}
	export PERF_COMPPERCENT=${PERF_COMPPERCENT:-'66'}
	export PERF_COMPCHUNK=${PERF_COMPCHUNK:-'4096'}
	export PERF_RUNTYPE=${PERF_RUNTYPE:-'nightly'}
	export PERF_NTHREADS=${PERF_NTHREADS:-'64 128'}
	export PERF_NTHREADS_PER_FS=${PERF_NTHREADS_PER_FS:-'0'}
	export PERF_SYNC_TYPES=${PERF_SYNC_TYPES:-'1'}
	export PERF_IOSIZES='8k'
fi

# Layout the files to be used by the readwrite tests. Create as many files
# as the largest number of threads. An fio run with fewer threads will use
# a subset of the available files.
export NUMJOBS=$(get_max $PERF_NTHREADS)
export FILE_SIZE=$((TOTAL_SIZE / NUMJOBS))
export DIRECTORY=$(get_directory)
log_must fio $FIO_SCRIPTS/mkfiles.fio

# Set up the scripts and output files that will log performance data.
lun_list=$(pool_to_lun_list $PERFPOOL)
log_note "Collecting backend IO stats with lun list $lun_list"
if is_linux; then
	typeset perf_record_cmd="perf record -F 99 -a -g -q \
	    -o /dev/stdout -- sleep ${PERF_RUNTIME}"

	export collect_scripts=(
	    "zpool iostat -lpvyL $PERFPOOL 1" "zpool.iostat"
	    "vmstat -t 1" "vmstat"
	    "mpstat -P ALL 1" "mpstat"
	    "iostat -tdxyz 1" "iostat"
	    "$perf_record_cmd" "perf"
	)
else
	export collect_scripts=(
	    "kstat zfs:0 1"  "kstat"
	    "vmstat -T d 1"       "vmstat"
	    "mpstat -T d 1"       "mpstat"
	    "iostat -T d -xcnz 1" "iostat"
	    "dtrace -Cs $PERF_SCRIPTS/io.d $PERFPOOL $lun_list 1" "io"
	    "dtrace  -s $PERF_SCRIPTS/profile.d"                  "profile"
	)
fi

log_note "Random reads and writes with $PERF_RUNTYPE settings"
do_fio_run random_readwrite_fixed.fio false true
log_pass "Measure IO stats during random read and write load"
