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

	LOG("Driver object @ %p", DriverObject);

	auto driverName = INIT_USTRING(L"\\Driver\\colonelDriver");
	auto res = KFNs::pIoCreateDriver(&driverName, &Driver::MainEntryPoint);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: IoCreateDriver failed with status 0x%X", res);
	} else {
		LOG("Driver created successfully");
	}

	return res;
}