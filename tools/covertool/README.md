
# SEGA SATURN SAROO COVER TOOL

This tool is designed to work with SEGA Saturn cover images stored in SAROO's `cover.bin` binary format.

The cover images are stored in a binary file named `cover.bin`, which contains an index of cover images.
Each cover image is represented by a header structure that contains metadata about the image,
such as the serial ID, image offset, dimensions, and a hash value.

The tool can operate in 3 modes: unpacking, packing, and disc hash.

## Unpacking

In unpacking mode, it reads the `cover.bin` file, extracts each cover image, and saves it as a BMP file.

## Packing

In packing mode, it reads a list of BMP files from a `.txt` file, generates a cover index, and writes the cover images into a new `cover.bin` file.
The cover images are stored in a specific RAW format, including a palette and pixel data.
When packing, the tool also detects duplicate images to reduce output size.

## Game Disc Hash

To identify Saturn game images, SAROO uses an [Adler32 hash](https://github.com/tpunix/SAROO/blob/65d8764c24aa3a55f6c57f05c45b97f550d1fb49/Firm_MCU/Saturn/saturn_utils.c#L702) of the disc's IP.BIN.

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

## Build

```sh
make
```

## Credits

SAROO Cover Tool by [Bucanero](https://github.com/bucanero)
