# Press F Ultra

A Fairchild Channel F emulator for Nintendo 64.

![image](https://github.com/celerizer/Press-F-Ultra/assets/33245078/146d3645-fd53-48a7-9996-04c933c66a20)

## Usage

### Nintendo 64 Flashcart

- Place the `sl31253.bin` and `sl31254.bin` Channel F BIOS images and any additional cartridge ROMs in a "press-f" subdirectory on the SD Card.
- Boot Press F and choose a game to play.

### Emulator

- Add the binary data of `sl31253.bin` and `sl31254.bin` to the source code of `main.c`, as `bios_a`, `bios_a_size`, `bios_b`, and `bios_b_size`.
- Build the ROM per the instructions below.
- Load in the Ares emulator.

## Building

- Set up a [libdragon environment](https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon).
- Clone the project and the core emulation submodule:
```sh
git clone https://github.com/celerizer/Press-F-Ultra.git --recurse-submodules
```
- Run `make`.
