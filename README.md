# midi2vgm

Convert MIDI files into VGM files.

## Usage
Currently, OPL3 is the only target - please use the `midi2vgm_opl3` command. Hopefully we can add more targets in the future, like YM2612 or 10x SN76489.

```
midi2vgm_opl3 - Convert MIDI files to OPL3 VGM files
Usage:
  midi2vgm_opl3 [OPTION...]

 Main options:
  -h, --help               Show this help
      --show-banks         Show available banks (patch sets)
      --show-vol-models    Show available volume models
      --bank arg           Bank (patch set) (default: 58)
      --vol-model arg      Volume model (default: 0)
      --vgm-title-en arg   VGM Meta: Title EN
      --vgm-title arg      VGM Meta: Title
      --vgm-album-en arg   VGM Meta: Album EN
      --vgm-album arg      VGM Meta: Album
      --vgm-system-en arg  VGM Meta: System EN
      --vgm-system arg     VGM Meta: System
      --vgm-author-en arg  VGM Meta: Author EN
      --vgm-author arg     VGM Meta: Author
      --vgm-date arg       VGM Meta: Date
      --vgm-conv-by arg    VGM Meta: Converted By
      --vgm-notes arg      VGM Meta: Notes
      --in arg             Input file
      --out arg            Output file
```


## Build
### Requirements
- C++14 compatible compiler
- CMake 3.14+
- Git
- Working Internet connection

### Example for Debian/Ubuntu
Please run with root privileges.
```shell
apt install build-essential git cmake
git clone https://github.com/SudoMaker/midi2vgm
cd midi2vgm
mkdir build; cd build
cmake ..
make
```

## Notes
The rendered VGM file is optimized for playing on real hardware (e.g. our [RetroWave OPL3 Express](https://shop.sudomaker.com/products/retrowave-opl3-express) or a SoundBlaster card) rather than an emulator. It's mostly about channel panning.

## License
AGPLv3

## Credits
The `midi2vgm_opl3` is based on the [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) project.
