#!/bin/bash
# Shell script to test if restore feature of BKPFS works properly!
# ********************************************************************

echo "**************************************************************"
echo "Shell script to test if restore feature of BKPFS works properly"
echo "=============================================================="

# *************************************************************************************************

echo "Testing: restore when the main file is not existing!"
echo "-------------------------------------------------"

if ./bkpctl -r 1 -f filenaaaaaame | grep --quiet "File Error: Input file does not exist"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

# **************************************************************************************************

echo "Testing: view when the main file has no backups!"
echo "------------------------------------------------"
echo "sample" > sample.txt
echo "sample" > sample.txt

if ./bkpctl -r 2 -f sample.txt | grep --quiet "Restore Backup: Success"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt

# **************************************************************************************************

echo "Testing: view when the main file has backups!"
echo "---------------------------------------------"
echo "sample" > sample.txt

if ./bkpctl -r 1 -f sample.txt | grep --quiet "Restore Backup: Success"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt
