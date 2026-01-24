#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <intrin.h>

#include "helpers.h"

#define COLONEL_DEBUG

extern "C" NTSTATUS NTAPI IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);

namespace KFNs
{
    inline decltype(&IoCreateDevice) pIoCreateDevice = nullptr;
    inline decltype(&IoCreateSymbolicLink) pIoCreateSymbolicLink = nullptr;
    inline decltype(&IoGetCurrentIrpStackLocation) pIoGetCurrentIrpStackLocation = nullptr;
    inline decltype(&IofCompleteRequest) pIofCompleteRequest = nullptr;
    inline decltype(&PsLookupProcessByProcessId) pPsLookupProcessByProcessId = nullptr;
    inline decltype(&MmCopyMemory) pMmCopyMemory = nullptr;
    inline decltype(&MmMapIoSpace) pMmMapIoSpace = nullptr;
    inline decltype(&IoCreateDriver) pIoCreateDriver = nullptr;
    inline decltype(&MmUnmapIoSpace) pMmUnmapIoSpace = nullptr;
    inline decltype(&DbgPrintEx) pDbgPrintEx = nullptr;
    inline decltype(&RtlInitUnicodeString) pRtlInitUnicodeString = nullptr;
    
    using GetAsyncKeyState_t = SHORT(NTAPI*)(ULONG vkCode);
    inline GetAsyncKeyState_t pGetAsyncKeyState = nullptr;

    inline PLIST_ENTRY pPsLoadedModuleList = nullptr;
    
    void Initialize();
}

#ifdef COLONEL_DEBUG
#define LOG(fmt, ...) \
    KFNs::pDbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
               "[COLONEL] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

union VirtualAddress;

namespace Driver
{
    namespace Codes
    {
        constexpr ULONG INITCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x775, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG READCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x776, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG WRITECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x777, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG GETBASECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x778, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG KEYSTATECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x779, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
    }

    struct Info_t
    {
        _In_ HANDLE processId;
        _In_ PVOID targetAddress;
        _In_ PVOID bufferAddress;
        _In_ SIZE_T bytesToRead;
        _Out_ SIZE_T bytesRead;

        // Added for key state IOCTL
        _In_ ULONG key; // input: virtual key code (VK_xxx)
        _Out_ BOOLEAN isDown; // output: true if currently pressed
    };

    NTSTATUS NTAPI MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath);

    NTSTATUS HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
    NTSTATUS HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
    NTSTATUS HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
    NTSTATUS HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp);

    NTSTATUS HandleInitRequest(Info_t* buffer);
    NTSTATUS HandleReadRequest(Info_t* buffer);
    NTSTATUS HandleWriteRequest(Info_t* buffer);
    NTSTATUS HandleGetBaseRequest(Info_t* buffer);

    UINT64 GetPML4Base();
    PVOID TranslateVirtualToPhysical(VirtualAddress virtualAddress);

    NTSTATUS ReadPhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
    NTSTATUS WritePhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten);

    // Key state functions
    NTSTATUS CacheKeyStatePhysAddr();
    NTSTATUS ReadKeyState(ULONG vkCode, PBOOLEAN isDown);

    static PEPROCESS targetProcess = nullptr;
}

union PTE
{
    UINT64 value;

    struct
    {
        UINT64 present : 1;
        UINT64 readWrite : 1;
        UINT64 userSupervisor : 1;
        UINT64 pageWriteThru : 1;
        UINT64 pageCacheDisable : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 pageSize : 1;
        UINT64 global : 1;
        UINT64 available : 3;
        UINT64 physBase : 40;
        UINT64 reserved : 11;
        UINT64 executeDisable : 1;
    } parts;
};

union VirtualAddress
{
    UINT64 value;

    struct
    {
        UINT64 offset : 12;
        UINT64 pt_index : 9;
        UINT64 pd_index : 9;
        UINT64 pdpt_index : 9;
        UINT64 pml4_index : 9;
        UINT64 reserved : 16;
    } parts;
};
