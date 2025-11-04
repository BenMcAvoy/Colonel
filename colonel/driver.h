#pragma once

#define MAX_NAME_LEN 256

namespace Driver {
	namespace Codes {
		constexpr ULONG INITCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x775, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // attach

		constexpr ULONG READCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x777, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // read
		constexpr ULONG WRITECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x778, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // write
	}

	struct Info_t {
		_In_    WCHAR processName[MAX_NAME_LEN];
		_In_    PVOID targetAddress;
		_Inout_ PVOID bufferAddress;
		_In_    SIZE_T bytesToRead;
		_Out_   SIZE_T bytesRead;
		_Out_   PVOID baseAddress;
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
