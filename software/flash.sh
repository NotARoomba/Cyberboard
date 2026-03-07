#!/bin/bash
sudo dfu-util -d 0483:df11 -a "@Internal Flash   /0x08000000/256*04Kg" -s 0x08000000:leave -D build/release/software.bin
