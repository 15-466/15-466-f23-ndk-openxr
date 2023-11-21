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
# android-26 should work acc'd to the OVR docs, but our Maekfile builds against 29:
$ ./cmdline-tools/latest/bin/sdkmanager --install 'platforms;android-29'
# "version 28.0.3 or later"; latest as of --list is build-tools;34.0.0-rc3 but:
$ ./cmdline-tools/latest/bin/sdkmanager --install 'build-tools;33.0.2'
# android NDK (no version given, this is latest as of this writing):
$ ./cmdline-tools/latest/bin/sdkmanager --install 'ndk;26.1.10909125'
```

Also download Meta's OpenXR SDK (from https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/) and extract into a sibling of this directory:
```
$ mkdir ovr-openxr-sdk
$ cd ovr-openxr-sdk
$ unzip ../downloads/ovr_openxr_mobile_sdk_59.0.zip #current version as of this writing
```

For **desktop** VR use, grab Khronos's OpenXR loader: (see the README.md for more build info; like non-linux build instructions, required packages to install)
```
$ git clone https://github.com/KhronosGroup/OpenXR-SDK openxr-sdk
$ cd openxr-sdk
$ mkdir -p build/linux
$ cd build/linux
$ cmake -DDYNAMIC_LOADER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ../..
```

Once all of that is done you should have these three as siblings:
```
15466-f23-ndk-openxr #this folder
android-sdk #android sdk, tools, etc
ovr-openxr-sdk #Meta's OpenXR SDK stuff
openxr-sdk #Knrono's OpenXR SDK (optional; for desktop use)
```

### SteamVR Linux Notes

Getting "failed to determine active runtime file path for this environment":

```
$ mkdir -p .config/openxr/1
$ ln -sf ~/.steam/steam/steamapps/common/SteamVR/steamxr_linux64.json ~/.config/openxr/1/active_runtime.json
```

Or you can just:
```
$ XR_RUNTIME_JSON=~/.steam/steam/steamapps/common/SteamVR/steamxr_linux64.json ./dist/game
```
(As per https://community.khronos.org/t/openxr-loader-how-to-select-runtime/108524)


I found that opening SteamVR, then closing SteamVR but leaving steam open resulted in OpenXR calls succeeding. But not having steam open at all or having steamvr open both resulted in CreateInstance failing.

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

```
#build and package:
$ node Maekfile.js dist-android/game.apk
```

NOTE: this build will sign the build with the keypair stored in the `android/` folder. For testing it's probably fine to just use the included keypair (which was made with `android/gen_key.sh`); be aware that this does mean that anyone with access to this repository can (if they have your development hardware) overwrite your test builds with their own without uninstalling. I.e., they can read data your test build writes on your test device if they have physical access to your test device.

## OpenXR Notes

## Installing on Android


Basic install + run workflow:
```
#from this folder, after building and packaging:

#install the app:
$ ../android-sdk/platform-tools/adb install -r dist-android/game.apk

#start the app:
$ ../android-sdk/platform-tools/adb shell am start --activity-clear-top -n "com.game_programming.ndk_openxr_example/android.app.NativeActivity"
```

For checking things are okay:
```
#print system log: (note -- can filter)
$ ../android-sdk/platform-tools/adb logcat
#version with some filtering:
$ ../android-sdk/platform-tools/adb logcat *:E OpenXR:I

#see what's installed:
$ ../android-sdk/platform-tools/adb shell pm list packages

#uninstall the app:
$ ../android-sdk/platform-tools/adb uninstall com.game_programming.ndk_openxr_example
```



## EXTRA NOTES

https://developer.oculus.com/documentation/native/android/mobile-native-manifest/

https://developer.android.com/ndk/guides/concepts#na

https://stackoverflow.com/questions/75463480/is-it-possible-to-use-android-ndk-without-nativeactivity


https://stackoverflow.com/questions/59504840/create-jni-ndk-apk-only-command-line-without-gradle-ant-or-cmake

https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md#Clang


https://stackoverflow.com/questions/64878248/switch-from-aapt-to-aapt2-for-native-app-packaging
