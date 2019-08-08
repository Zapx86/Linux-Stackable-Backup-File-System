#!/bin/bash
# Shell script to test if hide feature of BKPFS works properly!!!
# ********************************************************************

echo "**************************************************************"
echo "Shell script to test if hide feature of BKPFS works properly!!"
echo "=============================================================="

# **************************************************************************************************

echo "Testing: view when the main file has backups!"
echo "---------------------------------------------"
echo "sample" > sample.txt
echo "sample" > sample.txt
echo "sample" > sample.txt
echo "sample" > sample.txt
echo "sample" > sample.txt

if ls -altr /mnt/bkpfs | grep --quiet ".backup.sample*"; then
	echo "Test 01: ------------------------------------------------------------> Failed"
else
	echo "Test 01: ------------------------------------------------------------> Passed"
fi

./bkpctl -d A -f sample.txt
rm -rf sample.txt
