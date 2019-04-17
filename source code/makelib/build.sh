#!/bin/sh
cd /home/zlttest/workspace/project/MakeDefogLib
make
make install
cp libdefog2.so ~/workspace/imx6/build/rootfs_uClibc/usr/lib/ -fv
cd ~/workspace/imx6/
make
cd image/
tftp 192.168.0.109 <<eof
binary
put rootfs.squashfs.xz
quit
eof

