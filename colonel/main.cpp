#include "driver.h"

extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT  a1,
	_In_ PUNICODE_STRING a2
) {
	UNREFERENCED_PARAMETER(a1);
	UNREFERENCED_PARAMETER(a2);

	UNICODE_STRING driverName;
	RtlInitUnicodeString(&driverName, L"\\Driver\\colonelDriver");
	
	LOG("Creating driver with DriverName: %wZ", &driverName);
	auto res = IoCreateDriver(&driverName, &Driver::MainEntryPoint);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: IoCreateDriver failed with status 0x%X", res);
	} else {
		LOG("Driver created successfully");
	}

	return res;
}