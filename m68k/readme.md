# Motorola 68000 build

should work on System 6+ and System 7/8/9 using quickdraw

## build

```shell
# install Retro68
mkdir -p ~/github
cd ~/github
git clone git@github.com:autc04/Retro68.git
# some other commands to get it installed, this was built using commit 531a03f

# clone and build for PPC
cd ~/github/c_banks_flightsim/m68k/build
make clean
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/github/Retro68-build/toolchain/powerpc-apple-macos/cmake/retrocarbon.toolchain.cmake
make
```

this builds for 68k too!

```shell
cd ~/github/c_banks_flightsim/m68k/build
make clean
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/github/Retro68-build/toolchain/m68k-apple-macos/cmake/retrocarbon.toolchain.cmake
make
```

Should generate `banks.dsk` which has `banks` on it. There's a copy of this file in the repo that may or may not boot.