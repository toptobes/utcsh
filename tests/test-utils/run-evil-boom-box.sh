#!/bin/bash

ebb_dir="$(realpath tests/test-utils/p2a-ebb)"
ebb_name="libevilboombox.so"
inp_path="tests/test-specs/evilboombox/in"

ebb_out_dir=".ebb_files"   # Shared with libebb.so C code.

ebb_sys_fn="${ebb_out_dir}/.ebb_syscall_fired"
ebb_alloc_fn="${ebb_out_dir}/.ebb_alloc_fired"

# From Mircea Vutcovici under CCBYSA4: https://stackoverflow.com/a/22617858/2914377
function dump_stack(){
    local i=0
    local line_no
    local function_name
    local file_name
    while caller $i ;do ((i++)) ;done | while read line_no function_name file_name;do echo -e "\t$file_name:$line_no\t$function_name" ;done >&2
}

function internal_error(){
    echo "EBB: Internal error. Stack trace:"
    dump_stack
    echo "This may be caused by a configuration error in your project or it"
    echo "    may be a bug in the test suite. Ask the instructors if you are not sure."
    kill -SIGUSR1 $$
}

function die(){
    kill -"$1" $$
}

function enable_preloads(){
    export OLD_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
    export OLD_LD_PRELOAD="$LD_PRELOAD"
    export LD_LIBRARY_PATH="$ebb_dir:$LD_LIBRARY_PATH"
    export LD_PRELOAD="$ebb_name:$LD_PRELOAD"
}

function disable_preloads(){
    export LD_LIBRARY_PATH="$OLD_LD_LIBRARY_PATH"
    export LD_PRELOAD="$OLD_LD_PRELOAD"
    unset OLD_LD_LIBRARY_PATH
    unset OLD_LD_PRELOAD
}

# Build the libevilboombox.so shared object
pushd "$ebb_dir" || internal_error
make &> /dev/null || ( echo "Running make was unsuccessful." && internal_error )
popd > /dev/null || internal_error

# Clean the output directory.
function clean_out_dir(){
    disable_preloads
    rm -rf "$ebb_out_dir"
    mkdir -p "${ebb_out_dir}"
    if [ ! -d "$ebb_out_dir" ]; then
        echo "Could not create EBB output directory."
        die 10
    fi
    enable_preloads
}

## Checks that all UTCSH instances exited cleanly. If the "main" utcsh instance
# (the one that's a direct child) of this script exits abnormally, that should
# be picked up by this script. However, if a grandchild exits (e.g. while
# failing to use dup2() after fork()), it will leave behind a PID file in the 
# ebb directory instead. This function checks for those files, and kills
# this script (as if the child has aborted) when this happens.
function check_clean_exit() {
    sync
    for fn in "$ebb_out_dir"/*; do
        if [[ "${fn}" =~ [0-9]+ ]]; then
          echo "Found PID file--UTCSH process exited abnormally."
          # I am opting to die with SIGABRT, sinec this is much closer to what
          # would happen if we could do signal passthrough from grandchildren,
          # even though dying with SIGUSR1 would give more information.
          # die 10 # Die with SIGUSR1
          die 6 # Die with SIGABRT
        fi
    done
}

## If UTCSH dies with a signal, bash will report an exit number of 128 + signum
# We need to make sure the script also dies with that signal, so that the test
# driver can report that to the user. Exiting 1 would be incorrect because this
# would appear to be a correct exit to test that only requires no-signal exits
function mirror_signal_exit() {
    if [ "$1" -gt 128 ]; then
        signum=$(($1 - 128))
        echo "Got signal $signum while running $2 tests"
        echo "Signal occurred on the $3 function call"
        die $signum
    fi
}

function cleanup(){
    disable_preloads
    rm -rf "${ebb_out_dir}"
    exit
}
trap cleanup EXIT SIGINT SIGTERM

# Verify that our preloads are actually working by checking the PID maps. Grep
# will fail if we keep the preloads enabled, so we need to disable them before
# launching checks. Vulnerable to TOCTOU, but the best we can do for now.
enable_preloads
/bin/sleep 10 &
sleeppid=$!
disable_preloads
if [ -e /proc/$sleeppid/maps ]; then
    sleep 1
    if ! cat /proc/$sleeppid/maps | grep "$ebb_name" > /dev/null; then
        echo "EBB: library failed to preload. Please report this to course staff."
        internal_error
    fi
else
    echo -e "No /proc/$sleeppid/maps file found--cannot verify preload presence.\n"
    echo -e "Ensure you are running on a Linux system. If you are and this message"
    echo -e "is still appearing, please report this to course staff.\n"
    echo -e "Killing test process to ensure no false negatives."
    internal_error
fi

# Incredibly, killing this process normally is insufficient: because it runs
# with preloads enabled, the signal handler may experience an allocation/syscall
# error. It needs to be absolutely dead by this point or its PID file may be 
# spuriously generated later on, causing failures, hence the SIGKILL.
kill -9 $sleeppid
enable_preloads

# Run the syscall tests, incrementing the failing function by one each time,
# until we get a failure or until we no longer fire a failure
touch $ebb_sys_fn
ebb_ctr=0
while [ -f $ebb_sys_fn ]; do
    clean_out_dir
    EBB_SYSCALL_CTR=$ebb_ctr ./utcsh $inp_path
    exitnum=$?
    check_clean_exit
    mirror_signal_exit $exitnum syscall $ebb_ctr
    ebb_ctr=$((ebb_ctr + 1))
done
printf "%s\n" "Finished syscall tests with counter at ${ebb_ctr}."

# Do the same for allocation tests
touch $ebb_alloc_fn
ebb_ctr=0
while [ -f $ebb_alloc_fn ]; do
    clean_out_dir
    EBB_ALLOC_CTR=$ebb_ctr ./utcsh $inp_path
    exitnum=$?
    check_clean_exit
    mirror_signal_exit $exitnum allocation $ebb_ctr
    ebb_ctr=$((ebb_ctr + 1))
done
printf "%s\n" "Finished allocation tests with counter at ${ebb_ctr}."

exit 0
