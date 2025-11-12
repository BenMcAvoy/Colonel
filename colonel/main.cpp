#include "helpers.h"
#include "driver.h"

#include <vendor/skCrypter.h>

extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
) {
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	KFNs::Initialize();

	// Create a real driver object, this one is manually mapped and not real
	// (DriverObject is just 0x0 in this case, so we need to create a proper one)
	auto driverName = INIT_USTRING(L"\\Driver\\colonelDriver");
	auto res = KFNs::pIoCreateDriver(&driverName, &Driver::MainEntryPoint);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: IoCreateDriver failed with status 0x%X", res);
	} else {
		LOG("Driver created successfully");
	}

	return res;
}