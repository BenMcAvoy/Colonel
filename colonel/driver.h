#pragma once

#define COLONEL_DEBUG

#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <intrin.h>

#include "helpers.h"

#ifdef COLONEL_DEBUG
#define LOG(fmt, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
               "[COLONEL] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#define MAX_NAME_LEN 256

extern "C" {
	// IoCreateDriver
	NTSTATUS IoCreateDriver(
		_In_ PUNICODE_STRING DriverName,
		_In_ PDRIVER_INITIALIZE InitializationFunction
	);
}

union VirtualAddress {
	UINT64 value; // full 64-bit virtual address

	struct {
		UINT64 offset : 12; // page offset (4 KB pages)
		UINT64 pt_index : 9;  // Page Table
		UINT64 pd_index : 9;  // Page Directory
		UINT64 pdpt_index : 9;  // Page Directory Pointer Table
		UINT64 pml4_index : 9;  // PML4
		UINT64 reserved : 16; // Sign-extended bits for canonical addresses
	} parts;
};

namespace Driver {
	namespace Codes {
		constexpr ULONG INITCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x775, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // attach

		constexpr ULONG READCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x776, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // read
		constexpr ULONG WRITECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x777, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // write
	}

	struct Info_t {
		_In_    HANDLE processId;
		_In_    PVOID targetAddress;
		_Inout_ PVOID bufferAddress;
		_In_    SIZE_T bytesToRead;
		_Out_   SIZE_T bytesRead;
	};

	NTSTATUS NTAPI MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath);

	NTSTATUS HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
	NTSTATUS HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
	NTSTATUS HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);
	NTSTATUS HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp);

	NTSTATUS HandleInitRequest(Info_t* buffer);
	NTSTATUS HandleReadRequest(Info_t* buffer);
	NTSTATUS HandleWriteRequest(Info_t* buffer);

	UINT64 GetPML4Base(PEPROCESS tProc);
	PVOID TranslateVirtualToPhysical(UINT64 cr3, VirtualAddress virtualAddress);
	NTSTATUS ReadPhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
	NTSTATUS WritePhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten);

	static PEPROCESS targetProcess = nullptr;
} // namespace Driver

struct PageMapLevel4 {
    ULONGLONG PageMapLevel4Entry[512];
};
struct PageDirectoryPointer {
    ULONGLONG PageDirectoryPointerEntry[512];
};
struct PageDirectory {
    ULONGLONG PageDirectoryEntry[512];
};
struct PageTable {
    ULONGLONG PageTableEntry[512];
};

union PTE {
    UINT64 value;

    struct {
        UINT64 present          : 1;  // bit 0
        UINT64 readWrite        : 1;  // bit 1
        UINT64 userSupervisor   : 1;  // bit 2
        UINT64 pageWriteThru    : 1;  // bit 3
        UINT64 pageCacheDisable : 1;  // bit 4
        UINT64 accessed         : 1;  // bit 5
        UINT64 dirty            : 1;  // bit 6 (only for PDEs pointing to large page)
        UINT64 pageSize         : 1;  // bit 7: 0 = points to page table, 1 = 2MB page
        UINT64 global           : 1;  // bit 8 (ignored for PDE pointing to PT)
        UINT64 available        : 3;  // bits 9-11, OS-specific
        UINT64 physBase         : 40; // bits 12-51: PFN
        UINT64 reserved         : 11; // bits 52-62
        UINT64 executeDisable   : 1;  // bit 63
    } parts;
};
