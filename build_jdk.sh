#!/bin/bash

path_to_jdk20=/home/xiaojiawei/jdks/jdk-20

export DISABLE_HOTSPOT_OS_VERSION_CHECK=ok

bash configure --with-boot-jdk=${path_to_jdk20} --with-jvm-variants=server --with-target-bits=64
make  CONF=linux-x86_64-server-release images -j