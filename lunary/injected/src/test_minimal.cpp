#include <android/log.h>
#include <jni.h>

extern "C" __attribute__((visibility("default"))) jint
JNI_OnLoad(JavaVM* vm, void*) {
    __android_log_print(ANDROID_LOG_INFO, "LunaryTest", "JNI_OnLoad called - minimal test");
    return JNI_VERSION_1_6;
}
