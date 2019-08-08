#!/bin/bash
# Shell script to test if list newest of BKPFS works properly!!!!
# ********************************************************************

echo "**************************************************************"
echo "Shell script to test if list newest of BKPFS works properly!!!"
echo "=============================================================="

# *************************************************************************************************

echo "Testing: list when the main file is not existing!"
echo "-------------------------------------------------"

if ./bkpctl -l N -f filenaaaaaame | grep --quiet "File Error: Input file does not exist"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

# **************************************************************************************************

echo "Testing: list when the main file has no backups!"
echo "------------------------------------------------"
echo "sample" > sample.txt
./bkpctl -d A -f sample.txt

if ./bkpctl -l N -f sample.txt | grep --quiet "Backups' Enumeration: Failure"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt

# **************************************************************************************************

echo "Testing: list when the main file has backups!"
echo "---------------------------------------------"
echo "sample" > sample.txt
echo "sample" > sample.txt

if ./bkpctl -l N -f sample.txt | grep --quiet "Backups' Enumeration: Success"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt
