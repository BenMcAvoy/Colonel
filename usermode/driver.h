#pragma once

#include <Windows.h>

#include <string>
#include <string_view>
#include <stdexcept>
#include <system_error>

#include <Tlhelp32.h>

#include <format>

enum class DriverStatus {
	Success,
	DriverNotFound,
	FailedToAttach,
	ReadFailed,
	ReadPartial,
	WriteFailed,
	WritePartial,
	NotInitialized
};

class DriverException : public std::runtime_error {
public:
	DriverException(DriverStatus status, const std::string& message)
		: std::runtime_error(message), status_(status) {}
	
	DriverStatus status() const { return status_; }
	
	static std::string getStatusMessage(DriverStatus status) {
		switch (status) {
		case DriverStatus::Success: return "Success";
		case DriverStatus::DriverNotFound: return "Driver not found or failed to open";
		case DriverStatus::FailedToAttach: return "Failed to attach to target process";
		case DriverStatus::ReadFailed: return "Read operation failed";
		case DriverStatus::ReadPartial: return "Partial read - not all bytes were read";
		case DriverStatus::WriteFailed: return "Write operation failed";
		case DriverStatus::WritePartial: return "Partial write - not all bytes were written";
		case DriverStatus::NotInitialized: return "Driver not initialized";
		default: return "Unknown error";
		}
	}

private:
	DriverStatus status_;
};

template<typename T>
concept Addressable = std::is_pointer_v<T> ||
                      (std::is_integral_v<T> && sizeof(T) == 8);

class DriverManager {
private:
	static inline constexpr size_t MAX_NAME_LEN = 256;
	struct Info_t {
		_In_    WCHAR  processName[MAX_NAME_LEN]; // used by INIT
		_In_    UINT64 targetAddress;
		_Inout_ UINT64 bufferAddress;
		_In_    UINT64 bytesToRead;
		_Out_   UINT64 bytesRead;
		_Out_   UINT64 baseAddress;              // returned by INIT (optional)
	};

public:
	DriverManager(std::string_view driverName);
	~DriverManager() {
		if (hDriver_) {
			CloseHandle(hDriver_);
		}
	}

	DriverManager(const DriverManager&) = delete;
	DriverManager& operator=(const DriverManager&) = delete;

	bool isValid() const { return hDriver_ != nullptr; }
	DWORD getTargetPID() const { return targetPID_; }

	DriverStatus attachToProcess(std::string_view procName, uintptr_t* outBaseAddress = nullptr);

	template <typename T, Addressable U>
	T read(U address, DriverStatus* outStatus = nullptr) {
		if (!hDriver_) {
			if (outStatus) *outStatus = DriverStatus::NotInitialized;
			throw DriverException(DriverStatus::NotInitialized, 
				"Driver not initialized - cannot perform read operation"); 
		}

		T buffer{};
		Info_t info{};

		if constexpr (std::is_pointer_v<U>) {
			info.targetAddress = reinterpret_cast<UINT64>(address);
		} else {
			info.targetAddress = static_cast<UINT64>(address);
		}

		info.bufferAddress = reinterpret_cast<UINT64>(&buffer);
		info.bytesToRead = sizeof(T);
		info.bytesRead = 0;
		
		DWORD bytesReturned = 0;
		BOOL result = DeviceIoControl(
			hDriver_,
			READCODE,
			&info,
			sizeof(Info_t),
			&info,
			sizeof(Info_t),
			&bytesReturned,
			NULL
		);
		
		if (!result) {
			DWORD error = GetLastError();
			if (outStatus) *outStatus = DriverStatus::ReadFailed;
			throw DriverException(DriverStatus::ReadFailed,
				std::format("DeviceIoControl failed with error code: {} (0x{:X})", error, error));
		}
		else if (info.bytesRead != sizeof(T)) {
			if (outStatus) *outStatus = DriverStatus::ReadPartial;
			throw DriverException(DriverStatus::ReadPartial,
				std::format("Partial read: expected {} bytes, got {} bytes", sizeof(T), info.bytesRead));
		}
		
		if (outStatus) *outStatus = DriverStatus::Success;
		return buffer;
	}

	template <typename T, Addressable U>
	DriverStatus write(U address, const T& data) {
		if (!hDriver_) {
			throw DriverException(DriverStatus::NotInitialized,
				"Driver not initialized - cannot perform write operation");
		}

		Info_t info{};
		if constexpr (std::is_pointer_v<U>) {
			info.targetAddress = reinterpret_cast<UINT64>(address);
		} else {
			info.targetAddress = static_cast<UINT64>(address);
		}
		info.bufferAddress = reinterpret_cast<UINT64>(const_cast<T*>(&data));
		info.bytesToRead = sizeof(T);
		info.bytesRead = 0;
		
		DWORD bytesReturned = 0;
		BOOL result = DeviceIoControl(
			hDriver_,
			WRITECODE,
			&info,
			sizeof(Info_t),
			&info,
			sizeof(Info_t),
			&bytesReturned,
			NULL
		);
		
		if (!result) {
			DWORD error = GetLastError();
			throw DriverException(DriverStatus::WriteFailed,
				std::format("DeviceIoControl failed with error code: {} (0x{:X})", error, error));
		}
		else if (info.bytesRead != sizeof(T)) {
			throw DriverException(DriverStatus::WritePartial,
				std::format("Partial write: expected {} bytes, wrote {} bytes", sizeof(T), info.bytesRead));
		}
		
		return DriverStatus::Success;
	}
	
	void shutdown() {
		if (hDriver_) {
			DeviceIoControl(
				hDriver_,
				SHUTDOWNCODE,
				NULL,
				0,
				NULL,
				0,
				NULL,
				NULL
			);
		}
	}

private:
	HANDLE hDriver_;
	DWORD targetPID_;

	static inline constexpr DWORD INITCODE     = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x775, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static inline constexpr DWORD SHUTDOWNCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x776, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static inline constexpr DWORD READCODE     = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x777, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static inline constexpr DWORD WRITECODE    = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x778, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
};
