#!/bin/bash

#unset PYTHONPATH
#meson build -Dexamples=vdpa --buildtype=debug

source scl_source enable devtoolset-8

meson build -Dexamples=vdpa,vhost_blk -Dc_args='-DRTE_LIBRTE_VDPA_DEBUG' --debug
ninja -C build

