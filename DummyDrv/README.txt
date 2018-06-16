Mount system to RW: mount -o rw,remount /system
Mount system to RO: mount -o ro,remount /system

*REFERENCE*
https://github.com/lincolnhard/v4l2-framebuffer/blob/master/video_capture.c

=============* How to run ? *=================
1. mm build
2. copy 
	adb push ~/aosp_optee/out/target/product/hikey/system/bin/TestDummy /system/bin/
3. adb shell
	a) chmod +x /system/bin/TestDummy 
	b) ./system/bin/TestDummy
4. picocom
	picocom -b 115200 /dev/ttyUSB0 


=============* load Module *==================
cat proc/devices
	→ Check what the number is for chardev
mknod /dev/dummy_two c 244 0
cat /dev/chardev 
