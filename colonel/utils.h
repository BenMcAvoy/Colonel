#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>

#define COLONEL_DEBUG // FINDME

#ifdef COLONEL_DEBUG
#define LOG(fmt, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
               "[COLONEL] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation = 0,
	SystemProcessInformation = 5,
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER Reserved[3];
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	PVOID Reserved2;
	ULONG HandleCount;
	ULONG SessionId;
	PVOID Reserved3[2];
	SIZE_T VirtualMemorySize;
	SIZE_T PeakVirtualSize;
	SIZE_T WorkingSetSize;
	SIZE_T PeakWorkingSetSize;
	PVOID Reserved4[4];
	LONG BasePriority32;
	ULONG Reserved5;
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

extern "C" { // Undocumented windows internal functions (from ntoskrnl)
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess, PVOID TargetAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize);
	NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass, _Inout_ PVOID SystemInformation, _In_ ULONG SystemInformationLength, _Out_opt_ PULONG ReturnLength);
	NTKERNELAPI PVOID PsGetProcessSectionBaseAddress(PEPROCESS Process);
}

BOOLEAN CompareProcessName(_In_ const UNICODE_STRING* imageName, _In_ PCWSTR targetName);
NTSTATUS GetProcessByName(_In_ PCWSTR processName, _Out_ PEPROCESS* oProcess, _Out_opt_ PVOID* oBaseAddress = nullptr);
