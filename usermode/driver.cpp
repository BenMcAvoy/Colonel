#include "driver.h"

DriverManager::DriverManager(std::string_view driverName) 
	: hDriver_(nullptr), targetPID_(0) {
	std::string driverPath = std::format("\\\\.\\{}", driverName);
	hDriver_ = CreateFileA(
		driverPath.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hDriver_ == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		hDriver_ = nullptr;
		throw DriverException(DriverStatus::DriverNotFound,
			std::format("Failed to open driver '{}': error {} (0x{:X})", driverName, error, error));
	}
}

DriverStatus DriverManager::attachToProcess(std::string_view procName, uintptr_t* outBaseAddress) {
	// Kernel will enumerate the process by name; send the name only
	std::wstring wideName(procName.begin(), procName.end());

	Info_t info{};
	wcsncpy_s(info.processName, wideName.c_str(), MAX_NAME_LEN - 1);
	info.targetAddress = 0;
	info.bufferAddress = 0;
	info.bytesToRead = 0;
	info.bytesRead = 0;
	info.baseAddress = 0;

	DWORD bytesReturned = 0;
	BOOL result = DeviceIoControl(
		hDriver_,
		INITCODE,
		&info,
		sizeof(Info_t),
		&info,
		sizeof(Info_t),
		&bytesReturned,
		NULL
	);

	if (!result) {
		DWORD error = GetLastError();
		throw DriverException(DriverStatus::FailedToAttach,
			std::format("DeviceIoControl INITCODE failed with error: {} (0x{:X})", error, error));
	}

	if (outBaseAddress) {
		*outBaseAddress = static_cast<uintptr_t>(info.baseAddress);
	}

	return DriverStatus::Success;
}
