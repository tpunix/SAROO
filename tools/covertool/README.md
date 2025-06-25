
# SEGA Saturn SAROO Cover Tool

This tool is designed to work with SEGA Saturn cover images stored in SAROO's `cover.bin` binary format.

The tool can operate in 3 modes: unpacking, packing, and disc hash.

## Unpacking

In unpacking mode, it reads the `cover.bin` file, extracts each cover image, and saves it as a BMP file.

## Packing

In packing mode, it reads a list of BMP files from a `.txt` file, generates a cover index, and writes the cover images into a new `cover.bin` file.
The cover images are stored in a specific RAW format, including a palette and pixel data.
When packing, the tool also detects duplicate images to reduce output size.

## Game Disc Hash

To identify Saturn games and match the covers, SAROO uses an [Adler32 hash](#ipbin-hash) of the disc's data.

You can use SAROO Cover Tool to generate the correct disc Hash for your Saturn game image.

## Usage

```
SAROO Cover Tool v1.0.0 by Bucanero

Usage: ./covertool [option] cover_file
    -u          Unpack a cover.bin file to BMP images for each game
    -p          Pack BMP images and generate a cover.bin
    -i          Generate IP.BIN Hash for a Saturn game disc image

Example:
    ./covertool -u cover.bin
    ./covertool -p coverlist.txt
    ./covertool -i SaturnGame.bin
```

## Image Format

Cover images should follow these specifications:
- BMP format (8-bit, uncompressed)
- 256 colors palette
- Size: 128x128 or 128x192

## Build

```sh
make
```

## Technical Details

The cover images are stored in a binary file named `cover.bin`, which contains an index of cover images.
Each cover image is represented by a header structure that contains metadata about the image,
such as the serial ID, image offset, dimensions, and a hash value.

The index header structure is defined as:

```c
typedef struct cover_header
{
	char serial_id[12];
	uint32_t ip_hash; // disc IP.BIN adler32 hash
	uint32_t img_offset;
	uint16_t width;
	uint16_t height;
	char unused[8];
} cover_hdr_t;
```

The index supports a maximum of 16384 entries.

### IP.BIN Hash

To identify Saturn game images, SAROO uses an [Adler32 hash](https://github.com/tpunix/SAROO/blob/65d8764c24aa3a55f6c57f05c45b97f550d1fb49/Firm_MCU/Saturn/saturn_utils.c#L702) of the disc's IP.BIN data.

The hash is calculated using 64 bytes from the IP.BIN data, starting at offset `0x30`. For example:

```c
hash = adler32(ip_bin+0x30, 64);
```

## Credits

SAROO Cover Tool by [Bucanero](https://github.com/bucanero) - Licensed under GNU GPLv3
