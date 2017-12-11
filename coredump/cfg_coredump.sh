#!/bin/sh
#%p - insert pid into filename 添加pid
#%u - insert current uid into filename 添加当前uid
#%g - insert current gid into filename 添加当前gid
#%s - insert signal that caused the coredump into the filename 添加导致产生core的信号
#%t - insert UNIX time that the coredump occurred into filename 添加core文件生成时的unix时间
#%h - insert hostname where the coredump happened into filename 添加主机名
#%e - insert coredumping executable name into filename 添加命令名
#core-文件名-pid-时间戳
#gdb app core_app_xxxx
ulimit -c unlimited
ulimit -a 
echo "/home/core-%e-%p-%t" > /proc/sys/kernel/core_pattern

