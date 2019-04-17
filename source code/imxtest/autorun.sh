#!/bin/bash

# If this script is run with no parameters, it will run all the test ID's in
# autorun-suite.txt.  The autorun-suite.txt file also describes how each test
# ID is to be run.
#
# This script can be run with one or more test id's to run individual tests, i.e.:
#    ./autorun.sh FSL-UT-WDOG-0010 FSL-UT-KEYPAD-0010
#
# This script used to look for all the "autorun-*.sh" scripts and run them.  That
# has changed as described above.
#
#
# Each autorun-*.sh must exit with "exit 0" for pass or "exit 1" for fail.

source /unit_tests/test-utils.sh

run_test_id()
{
	SINGLE_STATUS=
	id="$1"

	if echo "$id"|grep -sq 'FSL-UT-' && ! echo "$id"|grep -sq '#'; then
		test="$(look_up_test_by_id $id)"
		ids="$(grep "$1" /unit_tests/autorun-suite.txt|cut -d: -f1)"
		if [ -n "$test" ]; then
			echo "***************************************"
			echo
			cd $(dirname "$test")
			echo "Test Ids: $ids"
			echo "Test case: $test"
			echo
			echo "Entering directory: $PWD"
			echo
			test=$(basename "$test")
			echo "$ids: Starting"
			echo "$test: Starting"

			let total_count=total_count+1
			if ! ./$test; then
				echo "$ids: Returns FAIL"
				echo "$test: Returns FAIL"
				let fail_count=fail_count+1
				SINGLE_STATUS=1
			else
				echo "$ids: Returns PASS"
				echo "$test: Returns PASS"
				let pass_count=pass_count+1
				SINGLE_STATUS=0
			fi
			echo
		fi
	fi
	cd $BASE
	return $SINGLE_STATUS
}

make_clean_todo_list()
{
    rm -f /unit_tests/autorun-todo.txt
    rm -f /unit_tests/autorun-suite.txt
    grep 'FSL-UT-' /unit_tests/all-suite.txt|egrep -v '#' | grep 'autorun' | grep "$(platform)" | \
	cut -d: -f1-2 > /unit_tests/autorun-suite.txt
    grep 'FSL-UT-' /unit_tests/autorun-suite.txt|egrep -v '#'| \
	cut -d: -f1|cut -d' ' -f1|sed -r 's,(FSL-UT-.*),\1 TODO,' > /unit_tests/autorun-todo.txt
}

run_todo_list()
{
	while read id; do
		state="$(echo "$id"|cut -d\  -f2)"
		if [ "$state" = TODO ]; then
			id=$(echo "$id"|cut -d\  -f1)
			sed -i "s,$id TODO,$id START," /unit_tests/autorun-todo.txt
			if ! run_test_id $id; then
				sed -i "s,$id .*,$id FAIL," /unit_tests/autorun-todo.txt
			else
				sed -i "s,$id .*,$id PASS," /unit_tests/autorun-todo.txt
			fi
		fi
	done < /unit_tests/autorun-todo.txt
	echo "autorun.sh: completed test suite - run_todo_list"
}

# If we are running on read-only rootfs, can't make todo list.
run_autorun_suite()
{
	while read id; do
		id=$(echo "$id"|cut -d\  -f1)
		run_test_id $id
	done < /unit_tests/autorun-suite.txt
	echo "autorun.sh: completed test suite - run_autorun_suite"
}

#=================================================================

total_count=0
pass_count=0
fail_count=0

BASE=$PWD

if [ $# -eq 0 ]; then
	if [ ! -f /unit_tests/autorun-todo.txt ]; then
		make_clean_todo_list
	fi
	if [ ! -f /unit_tests/autorun-todo.txt ]; then
		run_autorun_suite
	else
		run_todo_list
	fi
else
	while [ $# -gt 0 ]; do
		case $1 in
			FSL-UT-* ) run_test_id $1 ;;
			* ) echo "$0: invalid parameter" ;;
		esac
		shift
	done
fi

printf "Test cases run: %d  Pass: %d  Fail: %d\n\n" \
	$total_count $pass_count $fail_count

if [ $pass_count -ne 0 ] && [ $pass_count -eq $total_count ]; then
	echo "imx-test suite: PASS"
	exit 0
fi

if [ $total_count -eq 0 ]; then
	echo "imx-test suite: ALREADY DONE BEFORE."
	echo "Remove /unit_tests/autorun-todo.txt, and re-run autorun.sh"
	exit 0
fi

echo "imx-test suite: FAIL"
exit 1

