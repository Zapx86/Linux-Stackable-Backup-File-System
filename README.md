README - ASSIGNMENT 2
=====================

1. AUTHOR
=========
SAGAR JEEVAN

2. AUTHOR EMAIL ID
==================
sjeevan@cs.stonybrook.edu

3. PROBLEM STATEMENT
====================
To build a useful file system using stacking technologies. When you modify a file in traditional Unix file systems, there's no way to undo or recover a previous version of that file.  There's also no way to get older versions of the same file.  Being able to access a history of versions of file changes is very useful, to recover lost or inadvertent changes.

The task is to create a file system that automatically creates backup versions of files, and also allows you to view versions as well as recover them.

4. FILES NEEDED
===============
/usr/src/hw2-sjeevan/CSE-506/README.md (contains information on how the BKPFS works)
/usr/src/hw2-sjeevan/CSE-506/bkpctl.c (User File)
/usr/src/hw2-sjeevan/CSE-506/bkpctl.h (User Header file)
/usr/src/hw2-sjeevan/CSE-506/Makefile (contains commands to compile files)
/usr/src/hw2-sjeevan/include/linux/custom_ioctl.h (Common File between user and kernel)
/usr/src/hw2-sjeevan/CSE-506/run_test (runs 10 test scripts)
/usr/src/hw2-sjeevan/CSE-506/test*.sh (15 test scripts)

5. USAGE
========
./bkpctl -[l|d|v|r] [A|N|O|num] -f filename

[-l]: list the backup file versions
    * A - list all the versions of the main file
    * N - list the newest version of the main file
    * O - list the oldest version of the main file

[-d]: delete backup file/s
    * A - delete all the version of the main file
    * N - delete the newest version of the main file
    * O - delete the oldest version of the main file
    * num - delete the "nth" version of the main file

[-v]: view the contents of a backup file
    * N - view the newest version of the main file
    * O - view the oldest version of the main file
    * num - view the "nth" version of the main file

[-r]: restore the contents of a backup file to the main file
    * N - restore the newest version of the main file
    * O - restore the oldest version of the main file
    * num - restore the "nth" version of the main file

filename: the main filename

6. Backup File System DESIGN
============================
    6.1 USER-LAND
    =============
    1. There is a user-space file (bkpctl.c) that invokes specific operation requested.

    2. The user parameters is taken from the command line arguments. The getopt command is used to parse the input. The user can input the parameters as given below (examples).
        * ./bkpctl -l A -f filename [list all versions of the file filename]
        * ./bkpctl -d A -f filename [delete all versions of the file filename]
        * ./bkpctl -v Number -f filename [view nth version of the file filename]
        * ./bkpctl -r Number -f filename [restore nth version of the file filename]

        The arguments can be given in any order. For example, even the below commands are valid.

        * ./bkpctl -f filename -l A [list all versions of the file filename]
        * ./bkpctl -f filename -d A [delete all versions of the file filename]
        * ./bkpctl -f filename -v Number [view nth version of the file filename]
        * ./bkpctl -f filename -r Number [restore nth version of the file filename]

    3. The program performs various checks to validate the parameters passed by a user. For example,
        * The program checks if the user has input invalid parameters such as multiple/invalid flags, wrong arguments for a flag.

    4. After the parameters are checked if they conform to the syntax, now the program checks if they are indeed valid parameters.
    The checks below include for an input file.
        * Is the file path name valid?
        * Does the file exist?
        * Is the file a regular file?
        * Does the file have read permissions?

    5. Once the cases are passed, the user program checks for the operation requested. The arguments are passed on from user space to kernel by a custom structure available "custom_ioctl.h" header file. The contents of the header file are:

    #define LIST_VERSION 100034
    #define RESTORE_VERSION 100037
    #define VIEW_VERSION 100035
    #define DELETE_VERSION 100036

    typedef struct
    {
            char * buffer;
            int readsize;
            int operation_flag;
    } operationInfo;

    @buffer: for passing parameters from kernel to user. useful in list, and view of a backup files.
    @readsize: for letting the kernel know how many bytes are there in the file to read
    @operation_flag: to indicate A (all), O (oldest), N (newest), number (nth)

    7. If all of these succeed, the user program now calls kernel through ioctl. The parameters are passed on to the kernel program via a void pointer.

    6.2 KERNEL-LAND
    ===============
    The program creates a new structure from the "custom_ioctl.h" header file. The program then copies the parameters from user land structure via a function - copy_from_user.

    1. When to backup?
    ------------------
    These are the possible design strategies of when to back-up
        a. when a file is opened for writing.
        b. when the file is written to.
        c. create backup after every N writes to a file.
        d. after B bytes are modified in a file.

    I have designed the FS to backup ONLY "when the file is written to". I chose this design over others for the following reasons.
        a. when a file is opened for writing
        ------------------------------------
        I have not chosen this design because a file maybe opened for may reason, such as ls command, and others. Also, if a file is opened and not written, this creates multiple copy of the same content which is redundant and takes a lot of space.

        c. create backup after every N writes to a file.
        ------------------------------------------------
        I have not chosen this design because a file may undergo drastic changes over N versions. For example, if I create a backup for every 10 writes to the file, the first write and the 10th write may result in completely different files. This however has an advantage when a file is written to increase the content of a file rather than changing it.

        d. after B bytes are modified in a file.
        ----------------------------------------
        I have not chosen this design because creating a backup after B bytes are modified results in enormous amounts of backup version files getting created. This is clearly a bad idea keeping in mind the space needed to keep them.

    Thus, according to me, creating a backup file when a file is opened for writing creates a balance between options a, and d. Hence the design.

    2. What to backup?
    ------------------
    I am creating a backup for just regular files.

    3. where to store backups?
    --------------------------
    For simplicity, I am storing the backup files in the same directory where they exist. This allows the FS to not to worry about relative paths and other.
    This however creates an issue when a file has to be moved to a different directory. In such cases, the entire versions of backup files will need to be moved to the destination path. A better approach would be to create a backup directory where all the backup versions of all the files exists (not implemented). This reduces a lot of complexity and is easy to manage them.

    The backup files are stored in the directory of the main file. The naming scheme used here is given below.
        * backup.FILENAME.i
        where i is the ith version of the file.

    4. How to backup?
    -----------------
    I have used vfs_copy_file_range to create a copy of the main file. I have not chosen to use vfs_read, and then vfs_write because it is slower.

    5. Visibility Policy
    --------------------
    For a user, these files are not not visible, and are hidden. I have added a function filldir which gets redirected from readdir. Whenever a search is made for files, this function checks if the filename has ".backup." substring to it. If it does, the search returns NULL.

    6. Retention Policy
    -------------------
    I am keep N backups where N is specified during the mount. Whenever N+1th backup is needed, the oldest backup is deleted. The naming scheme is as follows below.
    For number of backups less than N,
        .backup.filename.1
        .backup.filename.2
            .......
        .backup.filename.N

    For number of backups greater than N say M,
        .backup.filename.(M-N)
        .backup.filename.(M-N + 1)
        .backup.filename.(M-N + 2)
                .....
        .backup.filename.(M)

    The version number of the files keep increasing. I have not decided to rename it backup to 1...N because doing so will result in losing track of versioning history. Every time renaming is done, the FS/user may assume that there were no modifications made ever for that file. Retaining this information will help users track their modifications of the file too.

    This may however increase the version number to a max of a certain datatype (i'm using unsigned long int which allows the FS to have maximum of 4,294,967,295 backups). I am also resetting the backup version number to 1...N when all of the backup files have been deleted.

    7. Version Management
    ---------------------
    These are the four supported version management options.
        * list all versions of a file
        -----------------------------
        This operation takes just the filename as a parameter. This function fetches the minimum, and current version of the file. A vfs_path_lookup is made ranging from minimum to current version to check if a backfile exists.

        * delete newest, oldest, or all versions.
        -----------------------------------------
        The delete operation takes two parameters, filename and a flag (Oldest, Newest, Num, All)
        * if the flag is All, all of the backups are deleted and the versioning scheme is reset (1..N).
        * else if version number is equal to minimum version, there are two cases.
            * current version == minimum version
            ====================================
            In this case, there is only a single backup file. So, the file is deleted and the versioning scheme is reset (1..N).

            * current version != minimum version
            ====================================
            In this case, the minimum version backup file is deleted and the minimum is set to the first version number that is greater than the current minimum version.
        * else if version number is equal to maximum version, there are two cases.
            * current version == maximum version
            ====================================
            In this case, there is only a single backup file. So, the file is deleted and the versioning scheme is reset (1..N).

            * current version != maximum version
            ====================================
            In this case, the maximum version backup file is deleted and the maximum is set to the first version number that is lesser than the current maximum version.
        * else, the version number is equal to either of the intermediate version. So, the file is just deleted.

        * view file version V, newest, or oldest.
        ----------------------------------------
        The restore operation takes two arguments, main file, and the file version number to view. The function checks for the existence of the file by vfs_path_lookup. If the backup file exist, chunks of content is read and passed to the user-land in sizes of 4kb.

        * restore newest version, oldest, or any version V.
        ---------------------------------------------------
        The restore operation takes two arguments, main file, and the file version number to restore to. First, the FS checks if a backup file for the version number exists. If it exists, the main file is truncated, and then contents from the backup file are copied to the main file by using vfs_copy_file_range. The backup file is retained and no backup file is created during this operation.

    8. Other Designs
    ----------------
        8.1 Backup Meta-data
        --------------------
        All the information that is needed to maintain backups for a file is stored in an extended attribute of a file. This allows the data to be persistent unlike keeping them in inode.i_private.

        The extended attributes are set for a file when it is opened for writing for the first time. Below is the list of extended attributes that are used by the FS.
            * Minimum Version (current)
            * Maximum Version (current)
            * Current Version
            * Number of Versions

7. TESTING
==========
The CSE-506 directory includes 15 test cases each named by test**.sh file. These are shell scripts that are used to test various workings of the program.
    * test01.sh - Shell script to test if the passed user arguments are indeed invalid
    * test02.sh - Shell script to test if list newest of BKPFS works properly
    * test03.sh - Shell script to test if list oldest of BKPFS works properly
    * test04.sh - Shell script to test if list ALL of BKPFS works properly
    * test05.sh - Shell script to test if delete newest of BKPFS works properly
    * test06.sh - Shell script to test if delete oldest of BKPFS works properly
    * test07.sh - Shell script to test if delete ALL of BKPFS works properly
    * test08.sh - Shell script to test if view newest of BKPFS works properly
    * test09.sh - Shell script to test if view oldest of BKPFS works properly
    * test10.sh - Shell script to test if view nth of BKPFS works properly
    * test11.sh - Shell script to test if restore newest of BKPFS works properly
    * test12.sh - Shell script to test if restore oldest of BKPFS works properly
    * test13.sh - Shell script to test if restore nth of BKPFS works properly.
    * test14.sh - Shell script to test if hide feature of BKPFS works properly (/mnt/bkpfs)
    * test15.sh - Shell script to test if hide feature of BKPFS works properly (lower FS)

    There is a need to mount the FS first to run these test cases and must be placed in root of BKPFS. To run the test cases, use 'sh run_test" on command line. This will run all the tests!

8. References
=============
1. https://opensourceforu.com/2011/08/io-control-in-linux/ (example of ioctl implementation)
2. The Linux kernel source code (Mostly used)
3. The Wrapfs kernel sources (Mostly used)
4. https://www.linuxjournal.com/article/6485 (writing a file system by Prof. Erez Zadok)
2. http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html [error codes]
3. https://www.linuxjournal.com/article/8110
4. https://www.linuxjournal.com/article/6930
5. https://elixir.bootlin.com/linux/latest/source/include/linux/fs.h [inode structure]
6. https://elixir.bootlin.com/linux/v4.20.6/source/include/linux/dcache.h [dentry structure]
7. https://stackoverflow.com/questions/24290273/check-if-input-file-is-a-valid-file-in-c
8. https://stackoverflow.com/questions/19598497/check-if-a-folder-is-writable
9. https://stackoverflow.com/questions/37897767/error-handling-checking-in-the-kernel-realm
