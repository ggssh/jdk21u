#!/bin/bash

path_to_jdk20=/home/yuanyizhe/temp/jdk-20

export DISABLE_HOTSPOT_OS_VERSION_CHECK=ok

bash configure --with-boot-jdk=${path_to_jdk20} --with-jvm-variants=server --with-target-bits=64 --with-debug-level=release

bear make CONF=linux-x86_64-server-release images JOBS=$(nproc)