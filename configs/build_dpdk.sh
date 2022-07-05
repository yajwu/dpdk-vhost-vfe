#!/bin/bash

#unset PYTHONPATH
#meson build -Dexamples=vdpa --buildtype=debug
meson build -Dexamples=vdpa,vhost_blk -Dc_args='-DRTE_LIBRTE_VDPA_DEBUG' --debug
ninja -C build
#cp cmd.vdpa ./build/examples/cmd -f

