#pragma once

#include <cstddef>
#include <cstdint>

namespace lunary::injected {

using Il2CppMethodPointer = void (*)();

struct Il2CppClass;
struct MethodInfo;

struct Il2CppType {
    void* data;
    std::uint32_t bits;
    std::uint32_t padding;
};

struct Il2CppRuntimeInterfaceOffsetPair {
    Il2CppClass* interfaceType;
    std::int32_t offset;
    std::int32_t padding;
};

struct Il2CppClassPart1 {
    void* image;
    void* gcDesc;
    const char* name;
    const char* namespaze;
    Il2CppType byvalArg;
    Il2CppType thisArg;
    Il2CppClass* elementClass;
    Il2CppClass* castClass;
    Il2CppClass* declaringType;
    Il2CppClass* parent;
    void* genericClass;
    void* typeMetadataHandle;
    void* interopData;
    Il2CppClass* klass;
    void* fields;
    void* events;
    void* properties;
    const MethodInfo** methods;
    Il2CppClass** nestedTypes;
    Il2CppClass** implementedInterfaces;
    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;
};

struct Il2CppClassPart2 {
    Il2CppClass** typeHierarchy;
    void* unityUserData;
    std::uint32_t initializationExceptionGCHandle;
    std::uint32_t cctorStarted;
    std::uint32_t cctorFinished;
    std::uint32_t padding0;
    std::size_t cctorThread;
    void* genericContainerHandle;
    std::uint32_t instanceSize;
    std::uint32_t actualSize;
    std::uint32_t elementSize;
    std::int32_t nativeSize;
    std::uint32_t staticFieldsSize;
    std::uint32_t threadStaticFieldsSize;
    std::int32_t threadStaticFieldsOffset;
    std::uint32_t flags;
    std::uint32_t token;
    std::uint16_t methodCount;
    std::uint16_t propertyCount;
    std::uint16_t fieldCount;
    std::uint16_t eventCount;
    std::uint16_t nestedTypeCount;
    std::uint16_t vtableCount;
    std::uint16_t interfacesCount;
    std::uint16_t interfaceOffsetsCount;
    std::uint8_t typeHierarchyDepth;
    std::uint8_t genericRecursionDepth;
    std::uint8_t rank;
    std::uint8_t minimumAlignment;
    std::uint8_t naturalAlignment;
    std::uint8_t packingSize;
    std::uint8_t bitflags1;
    std::uint8_t bitflags2;
};

struct Il2CppClass {
    Il2CppClassPart1 part1;
    void* staticFields;
    void* rgctxData;
    Il2CppClassPart2 part2;
};

struct MethodInfo {
    Il2CppMethodPointer methodPointer;
    Il2CppMethodPointer virtualMethodPointer;
    void* invokerMethod;
    const char* name;
    Il2CppClass* klass;
    const Il2CppType* returnType;
    const Il2CppType** parameters;
    const void* rgctxDataOrMetadata;
    const void* genericMethodOrContainer;
    std::uint32_t token;
    std::uint16_t flags;
    std::uint16_t iflags;
    std::uint16_t slot;
    std::uint8_t parametersCount;
    std::uint8_t bitflags;
    std::uint32_t padding;
};

struct ManagedArray {
    Il2CppClass* klass;
    void* monitor;
    void* bounds;
    std::uintptr_t maxLength;
    void* values[1];
};

struct ManagedList {
    Il2CppClass* klass;
    void* monitor;
    ManagedArray* items;
    std::int32_t size;
    std::int32_t version;
    void* syncRoot;
};

static_assert(offsetof(Il2CppClass, staticFields) == 0xB8);
static_assert(offsetof(Il2CppClass, part1.methods) == 0x98);
static_assert(offsetof(Il2CppClass, part2.methodCount) == 0x11C);
static_assert(offsetof(MethodInfo, name) == 0x18);
static_assert(offsetof(MethodInfo, parametersCount) == 0x52);
static_assert(offsetof(ManagedArray, values) == 0x20);
static_assert(offsetof(ManagedList, items) == 0x10);
static_assert(offsetof(ManagedList, size) == 0x18);

inline const MethodInfo* findMethod(Il2CppClass* klass, const char* name,
                                    std::uint8_t parameters) {
    if (!klass || !klass->part1.methods || !name) return nullptr;
    for (std::uint16_t index = 0; index < klass->part2.methodCount; ++index) {
        const MethodInfo* method = klass->part1.methods[index];
        if (!method || !method->name || method->parametersCount != parameters) continue;
        const char* left = method->name;
        const char* right = name;
        while (*left && *right && *left == *right) {
            ++left;
            ++right;
        }
        if (*left == '\0' && *right == '\0') return method;
    }
    return nullptr;
}

} // namespace lunary::injected
