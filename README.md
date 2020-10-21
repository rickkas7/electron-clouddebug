# Electron Cloud Debug

*Special code for debugging cloud connection issues with the Particle Electron*

## Wait!

There is a newer and better version of cloud debug in the Particle Github. You should use that version instead:

[https://github.com/particle-iot/cloud-debug](https://github.com/particle-iot/cloud-debug)

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

- You should have the [Particle CLI](https://docs.particle.io/guide/tools-and-features/cli/photon/) version 1.49.0 or later installed.
- To update the CLI, use the command: `particle update-cli`.

## To Install - Electron or E Series:

If your Electron or E Series is not running Device OS 1.4.2 or later you should upgrade. If you're not sure, it doesn't hurt to do this step:

Put the device in DFU mode (blinking yellow) by holding down RESET and MODE, releasing RESET while continuing to hold down MODE. The main status LED will blink magenta (blue and red at the same time), then yellow. Once blinking yellow, release SETUP.

From a Command Prompt or Terminal window:

```
particle update
```

Download the [electron.bin](https://github.com/rickkas7/electron-clouddebug/raw/master/electron.bin) file. Click on that link and then the Download button on the page that displays, don't just right click and Save Link As.

If device is not blinking yellow, repeat the steps above to put it back in DFU mode, then:

```
particle flash --usb electron.bin
```

The Electron will restart. Immediately open a serial window. For example, using the CLI:

```
particle serial monitor
```

For Windows 10, to copy and paste out of the Command Prompt window, press Control-M (Mark), click and the end and drag to the beginning of where you want to copy. Then press Enter to copy the text.

## Doing a carrier scan

If you are stuck at blinking green, it can be helpful to do a tower or carrier scan to see if any towers are visible.

Reset the Electron and immediately open the particle serial monitor.

The Electron will breathe white. Tap the MODE button once and a carrier scan will begin. You must do this within the first 10 seconds of startup otherwise the normal tests will be done.


## Running Advanced Tests

If you connect using particle serial monitor, the default options are used. If you want to use a custom APN or keep-alive you need to use a terminal program that allows you to send USB serial data, such as:

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
or tap the MODE button once to show carriers
```

If you do nothing, the t option (run normal tests) is run, which behaves like the previous version of cloud debug.

If you press c (show carriers at this location), the program will scan nearby towers and show the carriers, frequencies, and signal strength. This takes several minutes to run, which is why it's not done by default.

If you are using a 3rd-party SIM card, you can set the APN and keep-alive values using the a and k options, respectively.


## To Remove

Put the Electron in DFU mode (blinking yellow) by pressing RESET and SETUP. Release RESET and continue to hold down SETUP while the LED blinks magenta until it blinks yellow, then release SETUP.

```
particle flash --usb tinker
```

Or flash your own firmware instead.


