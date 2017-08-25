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

Meanwhile, it uses a special debug version of the system firmware, so there's additional debugging information generated as well.

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

The source code is [here](https://github.com/rickkas7/electron-clouddebug/blob/master/clouddebug-electron.cpp) so you can see how it works, but you kind of have to have a gcc-arm local development environment to get the system debug feature.

## Prerequisites 

- You should have the [Particle CLI](https://docs.particle.io/guide/tools-and-features/cli/electron/) installed
- You must have a working dfu-util


## To Install - Electron

Because both debug system firmware and user firmware are required to get full debugging information, and downloading and installing all three pieces manually is a pain, I have a combined binary that contains all three parts in a single file.

> Technical note: This is actually system-part1, system-part2 (0.5.3) and the user firmware binary concatenated, with some padding, into a single file. It's not a monolithic binary, so you can actually flash new regular (modular) user firmware on top of it at 0x80A0000 or even OTA and it will work properly. Also, it does not contain the boot loader, so it can be flashed using dfu-util.

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

## Testing with a 3rd-party SIM card

The pre-built binary only works with the Particle SIM card. If you are using a 3rd-party SIM card, the following technique can be used:

- Make sure you've upgraded your Electron to 0.6.0 or later. Earlier versions of system firmware won't produce the necessary debugging information.

- Open this project. It will open in Particle Build (Web IDE)

[https://go.particle.io/shared_apps/59a00660475d3e181b0005d1](https://go.particle.io/shared_apps/59a00660475d3e181b0005d1)

- Uncomment this line and edit it for your APN

```
STARTUP(cellular_credentials_set("broadband", "", "", NULL));
```

- Click on the Code icon (<>), then the cloud icon near the top of the pane next to the name to download the binary. Then flash this by USB.




