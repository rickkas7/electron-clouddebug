# Electron Cloud Debug

*Special code for debugging cloud connection issues with the Particle Electron*

## What is this?

This is a tool to debug cloud connection issues. It:

- Prints out cellular parameters (ICCID, etc.)
- Prints out your cellular carrier and frequency band information
- Pings your IP gateway
- Pings the Google DNS (8.8.8.8)
- Does a DNS server lookup of the device server (device.spark.io)
- Makes an actual cloud connection
- Acts like Tinker after connecting 
- Can print out information about nearby cellular towers

It uses a special debug version of the system firmware, so there's additional debugging information generated as well.

Here's a bit of the output log:

```
service rat=GSM mcc=310, mnc=11094, lac=2 ci=a782 band=GSM 850 bsic=3b arfcn=ed rxlev=40
neighbor 0 rat=GSM mcc=310, mnc=11094, lac=80592df ci=a56f band=GSM 850 bsic=18 arfcn=eb rxlev=37
neighbor 1 rat=GSM mcc=310, mnc=11094, lac=100 ci=a5f2 band=GSM 850 bsic=25 arfcn=b4 rxlev=22
    15.443 AT send      20 "AT+UPING=\"8.8.8.8\"\r\n"
    15.443 AT read  +   14 "\r\n+CIEV: 2,2\r\n"
    15.484 AT read OK    6 "\r\nOK\r\n"
ping addr 8.8.8.8=1
    15.484 AT send      31 "AT+UDNSRN=0,\"device.spark.io\"\r\n"
    17.204 AT read  +   67 "\r\n+UUPING: 1,32,\"google-public-dns-a.google.com\",\"8.8.8.8\",55,812\r\n"
    17.865 AT read  +   67 "\r\n+UUPING: 2,32,\"google-public-dns-a.google.com\",\"8.8.8.8\",55,651\r\n"
    17.936 AT read  +   27 "\r\n+UDNSRN: \"52.91.48.237\"\r\n"
    17.946 AT read OK    6 "\r\nOK\r\n"
```

The source code is [here](https://github.com/rickkas7/electron-clouddebug/blob/master/clouddebug-electron.cpp) so you can see how it works. 

## Prerequisites 

- You should have the [Particle CLI](https://docs.particle.io/guide/tools-and-features/cli/electron/) installed
- You must have a working dfu-util


## To Install - Electron

Because both debug system firmware and user firmware are required to get full debugging information, and downloading and installing all four pieces manually is a pain, I have a combined binary that contains all four parts (3 system and 1 user) in a single file.

> Technical note: This is actually system-part1, system-part2, system-part3 (0.6.2 debug) and the user firmware binary concatenated, with some padding, into a single file. It's not a monolithic binary, so you can actually flash new regular (modular) user firmware on top of it at 0x80A0000 or even OTA and it will work properly. Also, it does not contain the boot loader, so it can be flashed using dfu-util.

Download the [combined-electron.bin](https://github.com/rickkas7/electron-clouddebug/raw/master/combined-electron.bin) file.

Put the Electron in DFU mode (blinking yellow) by pressing RESET and MODE. Release RESET and continue to hold down MODE while the LED blinks magenta until it blinks yellow, then release MODE.

Issue the command:

```
dfu-util -d 2b04:d00a -a 0 -s 0x8020000:leave -D combined-electron.bin
```

The Electron will restart. Immediately open a serial window. For example, using the CLI:

```
particle serial monitor
```

For Windows 10, to copy and paste out of the Command Prompt window, press Control-M (Mark), click and the end and drag to the beginning of where you want to copy. Then press Enter to copy the text.

## Running Tests

If you connect using particle serial monitor, the default options are used. If you want to use a custom APN, keep-alive, or do a cellular tower test, you need to use a terminal program that allows you to send USB serial data, such as:

- PuTTY or CoolTerm (Windows)
- screen (Mac or Linux)
- Particle Dev (Atom IDE) Serial Monitor
- Arduino serial monitor

There's more information on using serial terminals [here](https://github.com/rickkas7/serial_tutorial).

Once you connect to the serial terminal you can press Return or wait a few seconds and you should get a menu:

```
clouddebug: press letter corresponding to the command
a - enter APN for 3rd-party SIM card
k - set keep-alive value
c - show carriers at this location
t - run normal tests (occurs automatically after 10 seconds)
```

If you do nothing, the t option (run normal tests) is run, which behaves like the previous version of cloud debug.

If you press c (show carriers at this location), the program will scan nearby towers and show the carriers, frequencies, and signal strength. This takes several minutes to run, which is why it's not done by default.

If you are using a 3rd-party SIM card, you can set the APN and keep-alive values using the a and k options, respectively.


## To Remove

Using the Particle CLI, just update the system and user firmware binaries:

Put the Electron in DFU mode (blinking yellow) by pressing RESET and SETUP. Release RESET and continue to hold down SETUP while the LED blinks magenta until it blinks yellow, then release SETUP.

```
particle flash --usb tinker
```

Enter DFU (blinking yellow) mode again, then:

```
particle update
```

