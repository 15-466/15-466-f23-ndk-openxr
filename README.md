# 15-466 Demonstration Code - NDK + OpenXR

This demonstration code shows how to use a `Maekfile.js` to build with the NDK for Android devices, and how to use OpenXR on such devices.

Particularly, this code is designed to build for Meta Quest 3.

Based on [15-466-f23-base2](https://github.com/15-466/15-466-f23-base2) with modifications inspired by both the [Oculus OpenXR Mobile SDK](https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/) samples and the [OpenXR Source SDK](https://github.com/KhronosGroup/OpenXR-SDK-Source) samples.

## Setup

You will need to jump through some hoops to get your development environment set up, and to get your hardware ready for development.

### Software

Get the android sdk set up in a directory which is a sibling of this one using the `sdkmanager` utility. (Note, these instructions are based on https://developer.android.com/tools/sdkmanager .)


Start with a "command line tools only" package from https://developer.android.com/studio (scroll to the bottom). Now extract it and get it working:

```
$ mkdir android-sdk
$ cd android-sdk
$ unzip ../downloads/commandlinetools-linux-10406996_latest.zip
$ cd cmdline-tools
$ mkdir latest
#will complain about moving latest into itself, which is fine:
$ mv * latest

```

Now get the things that the ovr SDK says we need (as per https://developer.oculus.com/documentation/native/android/mobile-studio-setup-android/ ):
```
#from android-sdk:
$ ./cmdline-tools/latest/bin/sdkmanager --install 'platforms;android-26'
# "version 28.0.3 or later"; latest as of --list is build-tools;34.0.0-rc3 but:
$ ./cmdline-tools/latest/bin/sdkmanager --install 'build-tools;28.0.3'
# android NDK (no version given, this is latest as of this writing):
$ ./cmdline-tools/latest/bin/sdkmanager --install 'ndk;26.1.10909125'
```

Also download Meta's OpenXR SDK (from https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/) and extract into a sibling of this directory:
```
$ mkdir ovr-openxr-sdk
$ cd ovr-openxr-sdk
$ unzip ../downloads/ovr_openxr_mobile_sdk_57.0.zip #current version as of this writing
```

Once all of that is done you should have these three as siblings:
```
15466-f23-ndk-openxr #this folder
android-sdk #android sdk, tools, etc
ovr-openxr-sdk #Meta's OpenXR SDK stuff
```

### Hardware

Set up your Quest 3 for development, as per https://developer.oculus.com/documentation/native/android/mobile-device-setup/ .
- create a developer acct: https://developer.oculus.com/manage/organizations/create/
  - might require setting up 2FA at: https://developer.oculus.com/manage/verify/
- from oculus phone app: headset -> settings -> developer mode [set to: on]
- connect via USB C (the included white cable works, though is annoyingly short) and allow data (notification will pop up in headset)


You can now connect to the headset via the `adb` tool:
```
#from android-sdk:
$ ./platform-tools/adb devices
# shows "unauthorized" and causes the "allow debugging" dialog to pop on the headset
# if dialog accepted, says "device" next to the device (maybe that's the device name?)
$ ./platform-tools/adb devices
# if you like to explore a bit, get a shell on your device:
$ ./platform-tools/adb shell
# interesting things include `top` (see what's running), `cat /proc/cpuinfo` (learn more about the cpu)
#(ctrl-d to exit)
```

## Building


...
