#!/bin/sh
cp defog.json platform
cd platform
cp mxc_v4l2_tvin.out ~/workspace/imx6/build/rootfs_uClibc/software
cp defog.json ~/workspace/imx6/build/rootfs_uClibc/software
tftp 192.168.0.109 <<eof
binary
put mxc_v4l2_tvin.out
put defog.json
quit
eof
