# ipled - a versatile LED strip controller

ipled is a hardware controller for WS2812 LEDs and strips based on the mainstream STM32F103 micro controller.

It includes a 5 volts, 5/6 amps switching converter to operate from various types of rechargeable battery packs and it supports up to 6 strips that are operated in parallel for improved frame/refresh rates. The number of strips origininally resulted from the very first design case of this controller, being a walking act costume. There were two arms, two legs, a tail and headdress, and supporting six individual strips was a solution to avoid having to chain the strips one after another.

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


## History

This project dates back to 2017 when a colleague was approached by local artist who needed illumination for his latest work. Said artist had built multiple rings from Styrofoam with diameters of about 2 metres. The rings have thin legs with floats attached to it so they would hover some 80 centimetres above the surface of a small lake. Wihtin a cavity on the bottom face of the rings we fitted the WS2812 strips.

I think the first attempt to visualize what one could expect from the setup was in winter of 2018 when we gathered near the small creek *Ahr* in the lovely village *Schuld* where the artist lives (Germany). The creek was low on freezing cold water and we used some arbitrary LED strip controller bought on the Internet. I remember connecting the battery in reversed polarity, which ultimately ended this evening and sparked the idea to develop a more robust solution.

During the next year and a half I worked my way through the schematic capture and layout. STM32 was completely new to me as I mainly did AVR back then. Also I hadn't done any RF designs until then. The ability to properly debug on the STM32 (compared to the `printf`-type debuggin on the AVR) was a relief, yet somehow the silicon feels rather copy/pasted together and the silicon errata compensated for part of the relief. The RF part in contrast was quite enjoyable, mainly requiring sensible PCB artwork.

We ordered the PCBs via China and our employer thankfully allowed us to use their (manual) pick-and-placer and the vapour phase which greatly simplified the assembly of a batch of ten boards. Fortunately apart from the minor issues stated below all the boards worked at the first attempt.

The rest of the year was spent developing firmware and software. Then the COVID-19 struck.

We still managed to arrange (compliantly) a few meetings around a small lake to test our hardware. One time we had a trumpeter among us to experiment with musical backing, another time a local TV station stopped by for [a bit of footage for a feature](https://www.youtube.com/watch?v=kz8IDcXDvAs). You can briefly see me fumbling with my software [near the end](https://youtu.be/kz8IDcXDvAs?t=1722)...

Then sometime in 2021 when we were just about to get our act together for a *premiere* the infamous *Ahr valley flash flood* struck - it's the very creek we initially gathered at in 2018. Google it, you'll find [a lot of footage](https://www.theguardian.com/world/2022/jul/13/floods-then-and-now-photographs-germany-ahr-valley-flooding-disaster-july-2021) about it. The house of our artist was completely destroyed, there were railroad tracks literally hanging in the air 5 metres above ground and the village looked like a war zone. My colleage used to live there as well and only moved away shortly before. Yet somehow the Styrofoam rings that the artist kept in a shed next to his house survived.

Finally in September 2021 the installation successfully [premiered in Bremerhaven](https://www.bremerhaven.de/de/aktuelles/lichternacht-im-speckenbuetteler-park.119408.html).



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

The tool has a long history. Honour to whom honour is due, it introduced quite a few outstanding features during its heyday (1980s, 1990s I guess) and it left a mark in the electronics industry here. Nowadays, unfortunately, I would not recommend to start using it. There has been no major development for about a decade now, even if it's mainly the UI/rendering part that is in dire need of refurbishing. The software uses hardware dongles (Aladdin/Sentinel/Thales) and we all hope that ours won't fall apart soon.

I've included the gerber files, but there is not much more that I can export from Bartels (maybe DXF/SVG).



## Known limitations

End user documentation is rather thin.

The PCB design software is dead.

There is a minor issue with the `/LEDEN` and the buffer IC10. By asserting a high level on `/LEDEN` the 5V power supply is disabled to shut off the LED strips. This also removes power from the buffer IC10. Unfortunately then, the high level on `/LEDEN` will inadvertently/parasitically feed IC10.

This was originally inteded to shut off both power and data to the LED strips obviously. A reworkable fix is to cut the `/LEDEN` signal from IC10 and reroute its `/OE` signal to ground. This will remove the parasitic power from IC10 but leaves the outputs enabled when the power should be shut off. Then to correct this turn the data signals to a low level in software (already implemented in `leds.h`). This will ultimately remove power from the LEDs.

If I recall correctly the footprint for D45 and IC60 are too narrow. They are SOT23-6 and mistakenly derived them from the SOT23 footprint which is a little narrower.


## License and state

Software is GPL-3.0 unless otherwise noted. Portions of the CMSIS headers by ARM are Apache-2.0. The register sets by STM just say Copyright by STM. FatFS is under a 1-clause BSD license. The icons in the Qt applications are taken from the KDE Oxygen theme.

The circuit design is licensed under the CERN OHL-W-2.0.

The project has been sitting around for some time now. I would consider it to be mildly abandoned by now as I currently do not have the time and/or energy to continue working on it, unfortunately. Please feel free to send me a mail if you have questions or suggestions or if you can make use of it.



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

