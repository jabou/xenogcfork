# XenoGCfork

## Fork changes

### xenogcfork by jabou

Another fork of xenogcfork, originally published by emukidid, forked from Vingt-2.
Here are two major differences in this fork, as an add-on to Vingt-2:

* Added support for official Nintendo memory cards
* Updated compilation process

Official cards power up locked and return scrambled data until the DSP-assisted unlock handshake has been done (the same one libogc and games perform). Vingt-2 fork skipped it, so with an official card it always said `xeno.dol` could not be found. This (v2) performs the unlock, stores the card's flash ID in SRAM so Swiss can use the card afterward. Third-party cards work as before.
Tested with a DOL-020 card in both slots on a PAL GameCube. Without START or the disc, it boots normally.

### xenogcfork by Vingt-2

This is another fork of xenogcfork originally published by emukidid.

Here is the major difference in this fork:
* Replaced multi-game disc shell with a robust dol loader looking for a ``xeno.dol`` file in either memory card.
* You will still need a readable, bootable disc in your drive (can be anything).
* A .dol can easily be placed onto a memory card using swiss-gc (turning on file management in swiss's settings)

## Installation  
There are two ways to install this onto your XenoGC: The simplest and basically free method is to use the software updater. Unfortunately, you still need to take your cube apart to get to the xenogc modchip, and will need to solder the RST and GND pads of it to a switch, so the reset line of the atmega8 can be grounded on demand so it can be flashed.
The second method (typically if the first one failed ..., which it shouldn't) is to wire the 6 pins of an ISP flasher (a 6 pins usbtiny programmer or even Arduino Due, will work great), you can then use AVRDude on your computer to flash your atmega8 in mere seconds :).

*DISCLAIMER*: Flashing a software *always* comes with the risk of failure, and potential breakage of your device. In 99% of the time, a bad flash can be fixed by method #2, but it will require a tiny bit more soldering, and buying a proper programmer. 

## Flashing using the software flasher
This consists of using the XenoFlash.dol utility provided at the releases section. Note that this tool is a little finicky, however, I have been able to properly flash my chip with these exact steps about a dozen times without fail, so I'd say it's pretty safe (I repeatedly flashed the chip using my programmer, checked the version, then used these steps to flash it with the dol, and verified that it was indeed updated) so I'm confident this will work (on an actual XenoGC, that is, with an Atmega8 chip).
First, you will unfortunately still need to take your Gamecube apart, and to get to the optical drive. You will then need to solder a wire from the RST pad (any of the xeno letter !) and the ground pad to a Single Pole, Single Throw (aka... a switch).

<img width=600 src="software_installer_switch.jpg"/>

 Once that's done..

  * Place the XenoFlash.dol on your sd card / memcard, and boot load it with the loader of your choice.
  * Wait til the dol has fully booted and is displaying instructions.
  * Turn the switch ON (that is to drive RST to GND), the LED should turn OFF. If it doesn't, verify your wires.
  * Press Y, this will attempt to erase the flash.
  * Turn the switch back OFF (that is normal operation), the LED should be lit up again (red)
  * Turn the switch back ON (that is to drive RST to GND), the LED should turn off again.
  * Press Y, this will actually properly erase the flash.
  * Turn the switch OFF (that is, normal operation), the LED should NOT light up at this point.
  * Turn the switch back ON (that, is to drive RST to GND), the led should still NOT light up, obviously
  * Press A, this will flash the firmware onto the chip. This is a longer operation, wait until it says it's done.
  * Turn the switch back OFF, (normal operation) and the LED should shine again (red, not orange on an original XenoGC). If it does you can turn off the gamecube. If it doesn't, turn the switch back on again (that, is to drive RST to GND) and retry to flash until it's successful (as in the led turns on).

At this point, turning on your gamecube and pressing start should greet you with either your xeno.dol loading, or an error message if none was found in your memcards.

## Flashing using a USB programmer
You can also flash the chip directly using the ISP protocol to talk to the atmega8. The make file is already setup to use a usb programmer, I suggest you get yourself [one of these](https://www.amazon.com/USBtinyISP-Programmer-Bootloader-Download-Interface/dp/B01FDD4EP0/ref=pd_sbs_147_1/144-1489403-8576528?_encoding=UTF8&pd_rd_i=B01FDD4EP0&pd_rd_r=1a83009a-2ba1-4ee5-8a71-049222208b30&pd_rd_w=Rpcqt&pd_rd_wg=AUjDZ&pf_rd_p=b65ee94e-1282-43fc-a8b1-8bf931f6dfab&pf_rd_r=BW638ZGZ8ZXYEM6SSNVF&psc=1&refRID=BW638ZGZ8ZXYEM6SSNVF), [Arduino Due](https://www.amazon.com/Arduino-org-A000062-Arduino-Due/dp/B00A6C3JN2/ref=sr_1_1?crid=TK7V8PXGA8KP&dib=eyJ2IjoiMSJ9.f95OYzml5-bXlOyVU-7U6qymUHi9INUhwl5JYUdcOHFlCv9X4z4STO0pSuIumD77H369IICSwgRz6hWeBpdRdJGm0zoQ8q1c9bKipR_yFMQw6TK4VftV69oi5WyRvQS8YsDLi5XtbCcJMW3ihvwj5q2QukIZOOQqGu0_0gSUsx8Z-F4AmpZQOJoTnX9TI652M_euIUV4RG98qG24ZIQAbbVL97DJtrO-yJk_eqUlIls.7eeBgfSCLssVN0lJziOzzRwkrt2_kyPDC5i08XVxKaY&dib_tag=se&keywords=arduino+due&qid=1790176619&sprefix=arduino+du%2Caps%2C260&sr=8-1), or similar !
You will need to download [avrdude](https://www.nongnu.org/avrdude/) and place both files at the root of the repo. It can be used for any of mentioned programmers.
Now is solder time, I like using DuPont head style cables so I can tightly connect each pin of the programmer. Once you have soldered all the wires to the pcb and connected each pin to the programmer.

![Picture of ISP solder points](xenogc_ISP_solder_points.png)

Just enter make flash in a command line in the repo, (you mind need to install make on windows). This will flash xenoAT.hex that is located in XenoAT/. You can now put the optical drive back on its socket and give it a spin.

## To compile this yourself
Building v2 needs [devkitPPC](https://devkitpro.org/wiki/Getting_Started) with the `gamecube-dev` group and `ppc-zlib`, plus python3. WinAVR is not needed: the AVR code and the drivecode are taken verbatim from `XenoAT/Bin/XenoBoot.1.03a.v1.hex` (the v1 firmware) and only the loader payload is replaced.

```
make -C XenoShell                  # loader -> XenoShell/build/XenoBoot_packed.bin (must stay <= 4270 bytes)
python3 tools/build_firmware.py    # -> build/XenoAT.hex (ISP image) and XenoFlash/data/XenoAT.bin (flasher input)
make -C XenoFlash                  # -> XenoFlash/XenoFlash.dol (needs DEVKITPRO/DEVKITPPC set)
```

`build_firmware.py` prints the image CRC32, the flasher shows the same value on screen before writing.

## Known limitations
* The loader's status text uses NTSC (60 Hz) video timing regardless of the console region. Booting is not
  affected. A PAL-only CRT that cannot sync to 60 Hz may show a rolling picture instead of the text.
* `xeno.dol` is staged at 0x80800000 and is not bounded against the loader at 0x81500000, so a file larger
  than about 13 MB on a card would overwrite the loader. Swiss is under 1 MB.
