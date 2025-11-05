#include "driver.h"

using namespace Driver;

NTSTATUS Driver::HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);
	
	irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	LOG("CreateIO called. Device opened");
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	LOG("CloseIO called. Device closed");
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS NTAPI Driver::MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath) {
	UNREFERENCED_PARAMETER(regPath);

	UNICODE_STRING devName, symLink, symLinkGlobal;
	PDEVICE_OBJECT pDevObj = nullptr;

	RtlInitUnicodeString(&devName, L"\\Device\\colonelDevice");
	RtlInitUnicodeString(&symLink, L"\\??\\colonelLink");
	RtlInitUnicodeString(&symLinkGlobal, L"\\DosDevices\\Global\\colonelLink");

	LOG("Creating device: \\Device\\colonelDevice");
	auto status = IoCreateDevice( pDriver, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDevObj);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateDevice failed with status 0x%X", status);
		return status;
	}

	LOG("Creating symbolic link: \\??\\colonelLink");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateSymbolicLink(\\??) failed with status 0x%X", status);
		return status;
	}

	LOG("Creating symbolic link: \\DosDevices\\Global\\colonelLink");
	NTSTATUS statusGlobal = IoCreateSymbolicLink(&symLinkGlobal, &devName);
	if (!NT_SUCCESS(statusGlobal)) {
		LOG("ERROR: IoCreateSymbolicLink(\\DosDevices\\Global) failed with status 0x%X", statusGlobal);
		return statusGlobal;
	}

	// Enable buffered, this means that the I/O manager will allocate a system buffer for each I/O request
	SetFlag(pDevObj->Flags, DO_BUFFERED_IO);

	// Set all to unsupported (we manually set the ones we support below)
	for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		pDriver->MajorFunction[i] = Driver::HandleUnsupportedIO;

	// Set supported function callbacks
	pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = Driver::HandleIORequest;
	pDriver->MajorFunction[IRP_MJ_CREATE] = Driver::HandleCreateIO;
	pDriver->MajorFunction[IRP_MJ_CLOSE] = Driver::HandleCloseIO;
	pDriver->DriverUnload = NULL; // Unsupported

	ClearFlag(pDevObj->Flags, DO_DEVICE_INITIALIZING);
	LOG("Driver initialization completed successfully");

	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp) {
	UNREFERENCED_PARAMETER(pDev);

	irp->IoStatus.Information = sizeof(Driver::Info_t);

	auto stack = IoGetCurrentIrpStackLocation(irp);
	auto buffer = (Driver::Info_t*)irp->AssociatedIrp.SystemBuffer;

	if (!stack) {
		LOG("ERROR: Stack is NULL");

		irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return STATUS_INVALID_PARAMETER;
	}

	ULONG ctlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS status{};

	switch (ctlCode) {
	case Codes::INITCODE:
		status = HandleInitRequest(buffer);
		break;
	case Codes::READCODE:
		status = HandleReadRequest(buffer);
		break;
	case Codes::WRITECODE:
		status = HandleWriteRequest(buffer);
		break;
	}

	irp->IoStatus.Status = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS Driver::HandleInitRequest(Info_t* buffer) {
	if (!buffer) {
		LOG("ERROR: Buffer is NULL in INIT request");
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS getProcessStatus = PsLookupProcessByProcessId(buffer->processId, &Driver::targetProcess);

	if (!NT_SUCCESS(getProcessStatus)) {
		LOG("ERROR: GetProcessByName failed with status 0x%X", getProcessStatus);
		Driver::targetProcess = nullptr;
		return getProcessStatus;
	}

	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleReadRequest(Info_t* buffer) {
	if (!buffer) {
		LOG("ERROR: Buffer is NULL in READ request");
		return STATUS_INVALID_PARAMETER;
	}

	if (!Driver::targetProcess) {
		LOG("ERROR: No target process attached in READ request");
		return STATUS_INVALID_PARAMETER;
	}

	SIZE_T bytesRead = 0;
	auto res = MmCopyVirtualMemory(
		Driver::targetProcess,
		buffer->targetAddress,
		PsGetCurrentProcess(),
		buffer->bufferAddress,
		buffer->bytesToRead,
		KernelMode,
		&bytesRead
	);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: MmCopyVirtualMemory failed in READ request with status 0x%X", res);
		return res;
	}

	buffer->bytesRead = bytesRead;
	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleWriteRequest(Info_t* buffer) {
	if (!buffer) {
		LOG("ERROR: Buffer is NULL in WRITE request");
		return STATUS_INVALID_PARAMETER;
	}

	if (!Driver::targetProcess) {
		LOG("ERROR: No target process attached in WRITE request");
		return STATUS_INVALID_PARAMETER;
	}

	SIZE_T bytesWritten = 0;
	auto res = MmCopyVirtualMemory(
		PsGetCurrentProcess(),
		buffer->bufferAddress,
		Driver::targetProcess,
		buffer->targetAddress,
		buffer->bytesToRead,
		KernelMode,
		&bytesWritten
	);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: MmCopyVirtualMemory failed in WRITE request with status 0x%X", res);
		return res;
	}

	buffer->bytesRead = bytesWritten;
	return STATUS_SUCCESS;
}
