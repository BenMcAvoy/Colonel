#include "driver.h"

#include <vendor/skCrypter.h>
#include <vendor/kli.hpp>

using namespace Driver;

#define INITIALIZE(fn) do { \
	p##fn = KLI_FN(fn); \
	LOG("KLI Cache - %s initialized at %p", __FUNCTION__, #fn, p##fn); \
} while(0)

void KFNs::Initialize() {
	INITIALIZE(DbgPrintEx);
	INITIALIZE(IoCreateDevice);
	INITIALIZE(IoCreateSymbolicLink);
	INITIALIZE(IofCompleteRequest);
	INITIALIZE(PsLookupProcessByProcessId);
	INITIALIZE(MmCopyMemory);
	INITIALIZE(MmMapIoSpace);
	INITIALIZE(MmUnmapIoSpace);
	INITIALIZE(IoCreateDriver);
	INITIALIZE(DbgPrintEx);
	INITIALIZE(RtlInitUnicodeString);
}

NTSTATUS Driver::HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);
	
	irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	LOG("CreateIO called. Device opened");
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	LOG("CloseIO called. Device closed");
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS NTAPI Driver::MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath) {
	UNREFERENCED_PARAMETER(regPath);

	PDEVICE_OBJECT pDevObj = nullptr;

	auto devName = INIT_USTRING(L"\\Device\\colonelDevice");
	auto symLink = INIT_USTRING(L"\\??\\colonelLink");
	auto symLinkGlobal = INIT_USTRING(L"\\DosDevices\\Global\\colonelLink");

	LOG("Creating device: \\Device\\colonelDevice");
	auto status = KFNs::pIoCreateDevice(pDriver, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDevObj);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateDevice failed with status 0x%X", status);
		return status;
	}

	LOG("Creating symbolic link: \\??\\colonelLink");
	status = KFNs::pIoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateSymbolicLink(\\??) failed with status 0x%X", status);
		return status;
	}

	LOG("Creating symbolic link: \\DosDevices\\Global\\colonelLink");
	NTSTATUS statusGlobal = KFNs::pIoCreateSymbolicLink(&symLinkGlobal, &devName);
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
		KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);

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
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS Driver::HandleInitRequest(Info_t* buffer) {
	if (!buffer) {
		LOG("ERROR: Buffer is NULL in INIT request");
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS getProcessStatus = KFNs::pPsLookupProcessByProcessId(buffer->processId, &Driver::targetProcess);

	if (!NT_SUCCESS(getProcessStatus)) {
		LOG("ERROR: GetProcessByName failed with status 0x%X", getProcessStatus);
		Driver::targetProcess = nullptr;
		return getProcessStatus;
	}

	return STATUS_SUCCESS;
}

UINT64 Driver::GetPML4Base(PEPROCESS tProc) {
	constexpr UINT64 DirBaseOffset = 0x28; // KPROCESS::DirectoryTableBase

	auto dirBase = *reinterpret_cast<ULONG64*>(
		reinterpret_cast<UINT64>(tProc) + DirBaseOffset
	);

	return dirBase;
}

PVOID Driver::TranslateVirtualToPhysical(UINT64 pml4PhysBase, VirtualAddress va) {
	const auto pml4Index = va.parts.pml4_index;
	const auto pdptIndex = va.parts.pdpt_index;
	const auto pdIndex = va.parts.pd_index;
	const auto ptIndex = va.parts.pt_index;
	const auto pageOffset = va.parts.offset;

	SIZE_T bytesRead{};

	PTE pml4e{};
	{
		NTSTATUS st = ReadPhysicalMemory(
			reinterpret_cast<PVOID>(pml4PhysBase + (pml4Index * sizeof(UINT64))),
			&pml4e,
			sizeof(UINT64),
			&bytesRead);

		if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pml4e.parts.present) {
			LOG("PML4 entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
			return nullptr;
		}
	}

	PTE pdpte{};
	{
		NTSTATUS st = ReadPhysicalMemory(
			reinterpret_cast<PVOID>((pml4e.parts.physBase << 12) + (pdptIndex * sizeof(UINT64))),
			&pdpte,
			sizeof(UINT64),
			&bytesRead);

		if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pdpte.parts.present) {
			LOG("PDPT entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
			return nullptr;
		}
	}

	// Handle 1 GiB pages (PS bit in PDPTE)
	if (pdpte.parts.pageSize) {
		const auto physBase = (pdpte.parts.physBase << 12);
		const auto bigPageOffset = va.value & ((1ull << 30) - 1); // lower 30 bits

		return reinterpret_cast<PVOID>(physBase + bigPageOffset);
	}

	PTE pde{};
	{
		NTSTATUS st = ReadPhysicalMemory(
			reinterpret_cast<PVOID>((pdpte.parts.physBase << 12) + (pdIndex * sizeof(UINT64))),
			&pde,
			sizeof(UINT64),
			&bytesRead);

		if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pde.parts.present) {
			LOG("PD entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
			return nullptr;
		}
	}

	// Handle 2 MiB pages (PS bit in PDE)
	if (pde.parts.pageSize) {
		const auto physBase = (pde.parts.physBase << 12);
		const auto largePageOffset = va.value & ((1ull << 21) - 1); // lower 21 bits
		return reinterpret_cast<PVOID>(physBase + largePageOffset);
	}

	PTE pte{};
	{
		NTSTATUS st = ReadPhysicalMemory(
			reinterpret_cast<PVOID>((pde.parts.physBase << 12) + (ptIndex * sizeof(UINT64))),
			&pte,
			sizeof(UINT64),
			&bytesRead);

		if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pte.parts.present) {
			LOG("PT entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
			return nullptr;
		}
	}

	const auto phys = (pte.parts.physBase << 12) + pageOffset;
	return reinterpret_cast<PVOID>(phys);
}

NTSTATUS Driver::ReadPhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
	MM_COPY_ADDRESS fromAddress{};
	fromAddress.PhysicalAddress.QuadPart = reinterpret_cast<UINT64>(physicalAddress);
	return KFNs::pMmCopyMemory(buffer, fromAddress, size, MM_COPY_MEMORY_PHYSICAL, bytesRead);
}

NTSTATUS Driver::WritePhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten) {
	PHYSICAL_ADDRESS physAddr{};
	physAddr.QuadPart = reinterpret_cast<UINT64>(physicalAddress);
	VOID* mappedAddress = KFNs::pMmMapIoSpace(physAddr, size, MmNonCached);

	if (!mappedAddress) {
		LOG("ERROR: MmMapIoSpace failed in WRITE request");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlCopyMemory(mappedAddress, buffer, size);
	KFNs::pMmUnmapIoSpace(mappedAddress, size);

	if (bytesWritten) *bytesWritten = size;
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

	ULONG64 pml4Base = GetPML4Base(Driver::targetProcess);

	VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
	VOID* physAddress = TranslateVirtualToPhysical(pml4Base, va);

	// Prevent crossing page boundaries
	ULONG64 finalSize = Min(PAGE_SIZE - ((INT64)physAddress & 0xFFF), (LONGLONG)buffer->bytesToRead);
    SIZE_T bytesRead = NULL;

	NTSTATUS res = ReadPhysicalMemory(PVOID(physAddress), buffer->bufferAddress, finalSize, &bytesRead);
	if (!NT_SUCCESS(res)) {
		LOG("ERROR: ReadPhysicalMemory failed in READ request with status 0x%X", res);
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

	ULONG64 pml4Base = GetPML4Base(Driver::targetProcess);

	VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
	VOID* physAddress = TranslateVirtualToPhysical(pml4Base, va);

	// Prevent crossing page boundaries
	ULONG64 finalSize = Min(PAGE_SIZE - ((INT64)physAddress & 0xFFF), (LONGLONG)buffer->bytesToRead);
	NTSTATUS res = WritePhysicalMemory(PVOID(physAddress), buffer->bufferAddress, finalSize, &bytesWritten);

	if (!NT_SUCCESS(res)) {
		LOG("ERROR: MmCopyVirtualMemory failed in WRITE request with status 0x%X", res);
		return res;
	}

	buffer->bytesRead = bytesWritten;
	return STATUS_SUCCESS;
}
