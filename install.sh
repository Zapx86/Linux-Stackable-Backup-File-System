umount /mnt/ko2
rmmod bkpfs
insmod bkpfs.ko
mount -t bkpfs /test/ko2/ /mnt/ko2
