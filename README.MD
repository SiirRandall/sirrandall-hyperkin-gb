# Modified Firmware Release for RetronSQ (Based on Hyperkin Official Firmware Version 1.2)

## Forked From Hyperkin git repo
- This projected has been forked from [hyperkin-gb](https://github.com/hyperkin-game/hyperkin-gb) 

## Releases
- you can grab the new release [here](https://github.com/SiirRandall/sirrandall-hyperkin-gb/releases)


## Introduction

This is a refined firmware update specifically tailored for the RetronSQ, based on the official Hyperkin firmware version 1.2. This update enhances the functionality and user experience of the RetronSQ. Below is a detailed overview of the key features and improvements:

### Key Features and Improvements

#### Enhanced ROM Accessibility
- Improved access to a dedicated FAT32 volume for storing ROMs.
- Download game dumps and save from the ROM Directory.
- Easy accessibility from Windows or macOS.

#### Expanded Core Support
- Ability to enable additional cores integrated into the FAT32 partition.

#### Custom Shader Integration
- Enalbecustom shaders stored on the FAT32 drive.

#### Save State Functionality
- Modern save state feature accessible through the menu.
- Activate and use save states with a simultaneous press of the start and select buttons.

### Accessing the RetroArch Menu
- Easily accessible by pressing the start and select buttons simultaneously.
- This can be changed in the Retroarch menu. 

### Limitations and Concerns
- Aspect ratio adjustments remain a challenge due to hardware constraints and firmware attributes.
- Issue can be temporarily resolved by configuring aspect ratio settings in the Retroarch Video settings main menu.
- Uploading of saved data to cartridges is not supported(YET)

### Future Developments
- This release is the sole iteration of the modified firmware.
- Intent to compile an authentic Custom Firmware (CFW) in the future.
- Focus areas include automated FAT32 partition creation, aspect ratio consistency, and save data uploads to cartridges.
- No specific timeline for these developments due to professional and personal commitments.
- Make a custom menu when starting up if no cartrige is installed. 

### Adding Cores
- Download cores from [Libretro's nightly builds](https://buildbot.libretro.com/nightly/linux/armhf/latest/) for compatibility with the RetronSQ.

### Note
- Only two firmware versions available: 32GB and 64GB images. This will change in a future update.
- Future focus will be on developing the authentic Custom Firmware (CFW).

### Community Engagement
- Feel free to suggest improvements or report bugs.
- Any PRs are welcome
