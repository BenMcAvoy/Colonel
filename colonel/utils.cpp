#include "utils.h"

BOOLEAN CompareProcessName(_In_ const UNICODE_STRING* imageName, _In_ PCWSTR targetName) {
	if (!imageName || !targetName)
		return FALSE;

	if (imageName->Buffer == nullptr || imageName->Length == 0)
		return FALSE;

	UNICODE_STRING uTarget;
	RtlInitUnicodeString(&uTarget, targetName);

	return RtlEqualUnicodeString(imageName, &uTarget, TRUE);
}

NTSTATUS GetProcessByName(_In_ PCWSTR processName, _Out_ PEPROCESS* oProcess, _Out_opt_ PVOID* oBaseAddress) {
	if (!processName || !oProcess) {
		LOG("ERROR: Invalid parameters to GetProcessByName (missing processName or oProcess)");
		return STATUS_INVALID_PARAMETER;
	}

	*oProcess = NULL;

	if (oBaseAddress) {
		LOG("oBaseAddress provided, initializing to nullptr");
		*oBaseAddress = nullptr;
	}

	NTSTATUS status = STATUS_NOT_FOUND;

	// Get required buffer size for system information
	ULONG length = 0;
	NTSTATUS infoStatus = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &length);
	if (infoStatus != STATUS_INFO_LENGTH_MISMATCH) {
		LOG("ERROR: ZwQuerySystemInformation failed to get length, status=0x%X", infoStatus);
		return infoStatus;
	}

	// Allocate space
	PSYSTEM_PROCESS_INFORMATION buffer = (PSYSTEM_PROCESS_INFORMATION)ExAllocatePool2(
		POOL_FLAG_NON_PAGED | POOL_FLAG_NON_PAGED,
		length,
		'prcN'
	);

	if (!buffer) {
		LOG("ERROR: ExAllocatePool2 failed to allocate %lu bytes", length);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Get the system information
	infoStatus = ZwQuerySystemInformation(SystemProcessInformation, buffer, length, &length);
	if (!NT_SUCCESS(infoStatus)) {
		LOG("ERROR: ZwQuerySystemInformation failed, status=0x%X", infoStatus);
		ExFreePool(buffer);
		return infoStatus;
	}

	LOG("Searching for process: %ws", processName);

	PSYSTEM_PROCESS_INFORMATION entry = buffer;
	int processesChecked = 0;

	// Loop all processes
	while (TRUE) {
		processesChecked++;
		if (CompareProcessName(&entry->ImageName, processName)) {
			LOG("Matching process found: %wZ", &entry->ImageName, processName);

			PEPROCESS proc;
			if (NT_SUCCESS(PsLookupProcessByProcessId(entry->UniqueProcessId, &proc))) {
				*oProcess = proc; // Save the found process
				LOG("Found process: %wZ (PID: %llu)", &entry->ImageName, (UINT64)(ULONG_PTR)entry->UniqueProcessId);
	
				if (oBaseAddress) {
					auto baseAddress = PsGetProcessSectionBaseAddress(proc);
					LOG("Base address obtained via PsGetProcessSectionBaseAddress: 0x%p", baseAddress);
					*oBaseAddress = baseAddress;
				}

				status = STATUS_SUCCESS;
				break;
			}
		}

		if (entry->NextEntryOffset == 0)
			break;

		entry = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)entry + entry->NextEntryOffset);
	}

	LOG("Total processes checked: %d", processesChecked);

	ExFreePool(buffer);
	return status;
}
