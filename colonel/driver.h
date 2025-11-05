#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <intrin.h>

#define COLONEL_DEBUG

#ifdef COLONEL_DEBUG
#define LOG(fmt, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
               "[COLONEL] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#define MAX_NAME_LEN 256

extern "C" {
	// MmCopyVirtualMemory
	NTSTATUS MmCopyVirtualMemory(
		_In_  PEPROCESS FromProcess,
		_In_  PVOID FromAddress,
		_In_  PEPROCESS ToProcess,
		_In_  PVOID ToAddress,
		_In_  SIZE_T BufferSize,
		_In_  KPROCESSOR_MODE PreviousMode,
		_Out_ PSIZE_T NumberOfBytesCopied
	);

	// IoCreateDriver
	NTSTATUS IoCreateDriver(
		_In_ PUNICODE_STRING DriverName,
		_In_ PDRIVER_INITIALIZE InitializationFunction
	);
}

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

	static PEPROCESS targetProcess = nullptr;
} // namespace Driver
