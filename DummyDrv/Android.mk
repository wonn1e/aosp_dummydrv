LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_LDLIBS := -lc -llog

# give module name
LOCAL_MODULE := TestDummy

# list your C files to compile
LOCAL_SRC_FILES := program.c

# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

