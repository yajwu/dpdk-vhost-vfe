#!/bin/bash

#scl enable devtoolset-8 bash
[ -d bin ] || mkdir bin
pushd bin
source scl_source enable devtoolset-8


source scl_source enable devtoolset-8


#make clean
../configure --target-list=x86_64-softmmu --enable-kvm  --enable-debug --disable-seccomp
#../configure --target-list=x86_64-softmmu --enable-kvm

make -j24
popd
