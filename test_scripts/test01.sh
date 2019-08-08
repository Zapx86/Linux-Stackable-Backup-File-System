#!/bin/bash
# Shell script to test if the passed user arguments are indeed invalid!
# ********************************************************************

echo "**************************************************************"
echo "Shell script to test if the passed user arguments are invalid!!"
echo "=============================================================="

# *************************************************************************************************

echo "Testing: Passed user arguments for listing versions are invalid!"
echo "----------------------------------------------------------------"

if ./bkpctl -l A file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -l 10 file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 02: ------------------------------------------------------------> Passed"
else
	echo "Test 02: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -l -f file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 03: ------------------------------------------------------------> Passed"
else
	echo "Test 03: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -l A | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 04: ------------------------------------------------------------> Passed"
else
	echo "Test 04: ------------------------------------------------------------> Failed"
fi

echo "----------------------------------------------------------------"

# **************************************************************************************************

echo "Testing: Passed user arguments for deleting versions are invalid!"
echo "----------------------------------------------------------------"

if ./bkpctl -d A file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -d 10 file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 02: ------------------------------------------------------------> Passed"
else
	echo "Test 02: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -d -f file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 03: ------------------------------------------------------------> Passed"
else
	echo "Test 03: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -d A | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 04: ------------------------------------------------------------> Passed"
else
	echo "Test 04: ------------------------------------------------------------> Failed"
fi

echo "----------------------------------------------------------------"

# **************************************************************************************************

echo "Testing: Passed user arguments for viewing versions are invalid!"
echo "----------------------------------------------------------------"

if ./bkpctl -v A file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -v 10 file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 02: ------------------------------------------------------------> Passed"
else
	echo "Test 02: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -v -f file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 03: ------------------------------------------------------------> Passed"
else
	echo "Test 03: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -v A | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 04: ------------------------------------------------------------> Passed"
else
	echo "Test 04: ------------------------------------------------------------> Failed"
fi

echo "----------------------------------------------------------------"

# **************************************************************************************************


echo "Testing: Passed user arguments for restore versions are invalid!"
echo "----------------------------------------------------------------"

if ./bkpctl -r A file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 01: ------------------------------------------------------------> Passed"
else
	echo "Test 01: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -r 10 file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 02: ------------------------------------------------------------> Passed"
else
	echo "Test 02: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -r -f file | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 03: ------------------------------------------------------------> Passed"
else
	echo "Test 03: ------------------------------------------------------------> Failed"
fi

if ./bkpctl -r A | grep --quiet "Input Error: Improper Syntax (See Below)"; then
	echo "Test 04: ------------------------------------------------------------> Passed"
else
	echo "Test 04: ------------------------------------------------------------> Failed"
fi

echo "----------------------------------------------------------------"
# **************************************************************************************************
