#!/bin/bash
# Shell script to test if delete feature of BKPFS works properly!
# ********************************************************************

echo "**************************************************************"
echo "Shell script to test if delete feature of BKPFS works properly"
echo "=============================================================="

# *************************************************************************************************

echo "Testing: delete when the main file is not existing!"
echo "---------------------------------------------------"

if ./bkpctl -d A -f filenaaaaaame | grep --quiet "File Error: Input file does not exist"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

# **************************************************************************************************

echo "Testing: delete when the main file has no backups!"
echo "--------------------------------------------------"
echo "sample" > sample.txt

if ./bkpctl -d 2 -f sample.txt | grep --quiet "Backup Delete: Failure"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt

# **************************************************************************************************

echo "Testing: delete when the main file has backups!"
echo "---------------------------------------------"
echo "sample" > sample.txt
echo "sample" > sample.txt

if ./bkpctl -d A -f sample.txt | grep --quiet "Backup Delete: Success"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt
