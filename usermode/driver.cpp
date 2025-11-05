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

DWORD DriverManager::getPIDByName(std::string_view procName, uintptr_t* outBaseAddress) {
	auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 pe32{};
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(snap, &pe32)) {
		do {
			std::wstring wProcName(procName.begin(), procName.end());
			if (wProcName == pe32.szExeFile) {
				if (outBaseAddress) {
					auto modSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pe32.th32ProcessID);
					if (modSnap != INVALID_HANDLE_VALUE) {
						MODULEENTRY32 me32{};
						me32.dwSize = sizeof(MODULEENTRY32);
						if (Module32First(modSnap, &me32)) {
							*outBaseAddress = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
						}
						CloseHandle(modSnap);
					}
				}
				CloseHandle(snap);
				return pe32.th32ProcessID;
			}
		} while (Process32Next(snap, &pe32));
	}

	return 0;
}

DriverStatus DriverManager::attachToProcess(std::string_view procName, uintptr_t* outBaseAddress) const {
	DWORD pid = getPIDByName(procName, outBaseAddress);

	Info_t info{};
	info.targetPID = static_cast<UINT64>(pid);
	info.targetAddress = 0;
	info.bufferAddress = 0;
	info.bytesToRead = 0;
	info.bytesRead = 0;

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

	return DriverStatus::Success;
}
