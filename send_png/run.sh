#!/bin/bash

gcc -o generate_png.out generate_png.c -lavcodec -lavformat -lavutil -lswscale -lpng -lcurl 
gcc -o send_png.out send_png.c -lssl -lcrypto -Wdeprecated-declarations
gcc -o receive_png.out receive_png.c -lssl -lcrypto -Wdeprecated-declarations
gcc -o viewer.out viewer.c -lX11 -lImlib2
