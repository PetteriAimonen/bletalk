# Beginnings of a BGM113 project

Eventually this may become something useful, but for now it works as an example
of software development on BGM113 without being too tied to the Silicon Labs
files that have way too restricted licenses (which forbid open source
distribution of your application code).

This still uses the Silicon Labs SDK, but keeps it separate for the main source
tree that can be made open source.

"UG136: Silicon Labs Bluetooth C Application Developer's Guide" gives a good
overview of application structure, and this project somewhat follows it.

Currently there still seems to be some bug that causes it to end up in bootloader
more often than not.

## Building

To build this project, you need Silicon Labs Bluetooth SDK 2.5 installed.
You'll need to register on Silicon Labs website to download it.
By default the Makefile assumes the SDK path is
`$(HOME)/SimplicityStudio/developer/sdks/gecko_sdk_suite/v2.5`.
If yours is elsewhere, you could put a symlink there.

In addition you'll need `arm-none-eabi` toolchain from
https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads

After that `make` should build `bletalk.elf`, which contains 1st stage bootloader,
2nd stage bootloader, apploader, and application. (Yeah, seems silabs bootloader
structure is pretty complex.)

The `.elf` can be programmed onto BGM113 using any SWD-compatible programmer
device, I'm using stlink-v2 and openocd with `make program` command.

## License

Except where otherwise mentioned in file headers, MIT license applies:

> Copyright 2019 Petteri Aimonen
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

