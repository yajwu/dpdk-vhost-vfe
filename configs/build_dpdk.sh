#!/bin/bash

#unset PYTHONPATH
meson build -Dexamples=vdpa --buildtype=debug
ninja -C build
#cp cmd.vdpa ./build/examples/cmd -f

