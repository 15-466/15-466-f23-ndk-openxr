#!/usr/bin/sh

#based on https://stackoverflow.com/questions/59504840/create-jni-ndk-apk-only-command-line-without-gradle-ant-or-cmake

ANDROID_SDK=../../android-sdk
AAPT2=$ANDROID_SDK/build-tools/30.0.3/aapt2
AAPT=$ANDROID_SDK/build-tools/30.0.3/aapt
ZIPALIGN=$ANDROID_SDK/build-tools/30.0.3/zipalign
APKSIGNER=$ANDROID_SDK/build-tools/30.0.3/apksigner

mkdir -p ../objs/android/apk

"$AAPT2" compile -o ../objs/android/apk -v res/mipmap-mdpi/gp-icon.png
"$AAPT2" compile -o ../objs/android/apk -v res/mipmap-hdpi/gp-icon.png
"$AAPT2" compile -o ../objs/android/apk -v res/mipmap-xhdpi/gp-icon.png
"$AAPT2" compile -o ../objs/android/apk -v res/mipmap-xxhdpi/gp-icon.png
"$AAPT2" compile -o ../objs/android/apk -v res/mipmap-xxxhdpi/gp-icon.png

rm -f ../objs/android/unsigned.apk
rm -f ../objs/android/aligned.apk
rm -f ../dist-android/game.apk

"$AAPT2" link \
	-o ../objs/android/unsigned.apk \
	-I $ANDROID_SDK/platforms/android-29/android.jar \
	../objs/android/apk/mipmap-mdpi_gp-icon.png.flat \
	../objs/android/apk/mipmap-hdpi_gp-icon.png.flat \
	../objs/android/apk/mipmap-xhdpi_gp-icon.png.flat \
	../objs/android/apk/mipmap-xxhdpi_gp-icon.png.flat \
	../objs/android/apk/mipmap-xxxhdpi_gp-icon.png.flat \
	--manifest AndroidManifest.xml \
	-v

cp ../../ovr-openxr-sdk/OpenXR/Libs/Android/arm64-v8a/Release/libopenxr_loader.so ../objs/android/apk/lib/arm64-v8a/

cd ../objs/android/apk
"../../../android/$AAPT" add ../unsigned.apk lib/arm64-v8a/libgame.so
"../../../android/$AAPT" add ../unsigned.apk lib/arm64-v8a/libopenxr_loader.so
cd ../../../android

"$ZIPALIGN" -f -p 4 "../objs/android/unsigned.apk" "../objs/android/aligned.apk"

#keytool -genkeypair -keystore "../objs/android/keystore.jks" -alias androidkey -validity 10000 -keyalg RSA -keysize 2048 -storepass 'android'


mkdir -p ../dist-android

"$APKSIGNER" sign --ks ../objs/android/keystore.jks --ks-key-alias androidkey --out ../dist-android/game.apk ../objs/android/aligned.apk
