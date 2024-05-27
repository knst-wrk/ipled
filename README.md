# ipled - a versatile LED strip controller

ipled is a hardware controller for WS2812 LEDs and strips based on an STM32F1 micro controller.

It includes a 5 volts, 5 amps switching converter to operate from various types of rechargeable battery packs and it supports up to 6 strips that are operated in parallel for improved frame/refresh rates. The number of strips origininally resulted from the very first design case of this controller, being a walking act costume. There were two arms, two legs, a tail and headdress, and supporting six individual strips was a solution to avoid having to chain the strips one after another.

Features:
* microSD card slot
* reads TPM2 files, including a gentle modification that uses RLE compression
* hex switch to select various test patterns and modes
* RS485 (DMX or TPM2 stream) input
* one digital input (not used yet) for manual triggering
* six connectors for LED strips (5V - data - GND)
* 868MHz FSK wireless interface (Semtech SX1231)
* fits into a water tight 90mm by 36mm Hammond enclosure (1550Z102)
* flexible LED routing
* configuration via config file on SD card

The SD card and SX1231 parts do not use code from the ubiquitous Arduino SD and RFM69 libraries or clones thereof. I found those libraries to be rather, well, flakey, so I wrote something from scratch. Especially the SD card code seems to be quite robust; I've had a lot of SD cards that I can read but that did not seem to work in other commercial LED strip controllers. Using the hardware CRC unit we can get about 800kByte/s off the SD card.

The timing for the WS2812 is generated using hardware timers that trigger DMA transfers to transport bit patterns from RAM to the `BSR`/`BSRR` registers of the I/O ports.

All interfaces should be fault tolerant and ESD safe within reason. The battery input has a fuse, suppressor diode and reverse polarity protection. The way it works is like this:
* If <30V is applied correctly the controller works.
* If polarity is reversed but <30V then the T50 protects the circuit.
* If more than 30V is applied in any direction D50 will conduct and ultimately blow F50.

So what this means is that as long as you do not apply excessive voltage the fuse will stay alive you and once you realize that you reversed polarity you can correct that and you're good. Might save some nerves when you're in the field.


## Configuration

The controller is configured using a `index.txt` file in the root directory of the SD card. This file controls the radio interface, strip configuration and LED routing. Have a look at the provided example and the `config.c` source which contains the parser for it. If there are errors in the file the controller will append a comment with an error message to the file upon startup.

If the configuration file is not present the controller will start in standalone mode. There are some test patterns available then, as well as DMX or TPM2 input via RS485. Look in `main.c`.

The most important feature is the LED routing. Given six individual strips it is necessary to specify how the LED data from the TPM2 frame/DMX should be distributed to these strips. This is done via mapping in the configuration file. It looks a little weird but makes sense once you understand it. For instance, I once wanted to illuminate a ring. There were about 500 LEDs in it, and to reduce the voltage drop along one very long strip we cut it in half, and placed the controller just between the halves. So now I had one strip going clockwise and the other one going counter-clockwise. The according mapping looked like this:

    map {
        0: [0 .. 248] = rgb([0..$%3], [1..$%3], [2..$%3]);
        1: [248 .. 0] = rgb([747..$%3], [748..$%3], [749..$%3]);
    }

This uses the first 248x3 bytes of the frame to feed the LEDs on output 0 and then the next 248x3 bytes to feed the LEDs on output 1 but *in reverse order*. So from the outside (i.e., in the TPM2 files) everything looks like one continguous ring again.



## Radio control

When configured in `rx` mode the controller will listen for commands, whilst when configured in `tx` mode it will accept commands via RS485 and then transmit them. So the very same hardware is used both as the LED controller and the remote control.

RF frequency can be adjusted by configuration within limits; there is some filter circuitry on the PCB that is tuned for 868MHz right now. It can be tuned for other frequencies (presumably 433MHz). Maximum TX power is about 13dBm, which is both the maximum power output by the SX1231 chipset and the regulatory limit within Germany. There is a beacon mode that can be used to see how much range you get.

The `nodes/` directory contains a small Qt application that implements a remote control. It allows you to monitor remote controllers and start/stop playing TPM2 files from their SD cards.



## PCB layout

The PCB was laid out using [Bartels AutoEngineer](https://bartels.de/bae/bae_de.htm) simply because it was the tool that my employer was using at work at the time.

The tool has a long history. Honour to whom honour is due, it introduced quite a few outstanding features during its active life (1980s, 1990s I guess) and it left a mark in the electronics industry here. Nowadays, unfortunately, I would not recommend to start using it. There seems to be no major development going on for about a decade now, even if it's mainly the UI/rendering part that is in dire need of refurbishing.

I've included the gerber files, but there is not much more that I can export from Bartels.



## Known limitations

End user documentation is rather thin.

The PCB design software is dead.

There is a minor issue with the `/LEDEN` and the buffer IC10. By asserting a high level on `/LEDEN` the 5V power supply is disabled to shut off the LED strips. This also removes power from the buffer IC10. Unfortunately then, the high level on `/LEDEN` will inadvertently/parasitically feed IC10.

This was originally inteded to shut off both power and data to the LED strips obviously. A reworkable fix is to cut the `/LEDEN` signal from IC10 and reroute its `/OE` signal to ground. This will remove the parasitic power from IC10 but leaves the outputs enabled when the power should be shut off. Then to correct this turn the data signals to a low level in software (already implemented in `leds.h`). This will ultimately remove power from the LEDs.



## License and state

Software is GPL-3.0 unless otherwise noted. Portions of the CMSIS headers by ARM are Apache-2.0. The register sets by STM just say Copyright by STM. FatFS is under a 1-clause BSD license. The icons in the Qt applications are taken from the KDE Oxygen theme.

The circuit design is licensed under the CERN OHL-W-2.0.

The project has been sitting around for some time now. I would consider it to be a little abandoned by now. Feel free to send me a mail if you have questions or suggestions.



## What's inside

* `hw/` - PCB design files
    * `ipled.ddb` - design file
    * `ipled.pl` - parts list; chip sizes are metric, semiconductors as indicated
    * `ipled.plc` - pick and place coordinates
    * `cam/` - PCB gerber files
* `sw/` - firmware source code
    * `cmsis/` - CMSIS bonds from ARM (Apache-2.0) and STM
    * `dld/` - linker scripts
    * `ff/` - FatFS module and glue by ChaN (BSD)
* `lichter/` - test program for generating TPM2 files/streams
* `nodes/` - remote control application
* `index.txt` - sample configuration file

