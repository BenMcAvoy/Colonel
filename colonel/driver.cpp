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
	
	// Return not supported for all unsupported IOCTLs
	irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	// Simply acknowledge the create request
	LOG("CreateIO called. Device opened");
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(pDeviceObj);

	// Simply acknowledge the close request
	LOG("CloseIO called. Device closed");
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
}

NTSTATUS NTAPI Driver::MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath) {
	UNREFERENCED_PARAMETER(regPath);

	PDEVICE_OBJECT pDevObj = nullptr;

	// Create device and symbolic link strings
	auto devName = INIT_USTRING(L"\\Device\\colonelDevice");
	auto symLink = INIT_USTRING(L"\\??\\colonelLink");
	auto symLinkGlobal = INIT_USTRING(L"\\DosDevices\\Global\\colonelLink");

	// Create device so that we have a real pDevObj to work with
	LOG("Creating device: \\Device\\colonelDevice");
	auto status = KFNs::pIoCreateDevice(pDriver, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDevObj);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateDevice failed with status 0x%X", status);
		return status;
	}

	// Create symbolic links for user-mode access
	LOG("Creating symbolic link: \\??\\colonelLink");
	status = KFNs::pIoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		LOG("ERROR: IoCreateSymbolicLink(\\??) failed with status 0x%X", status);
		return status;
	}

	// Create symbolic link in the global namespace (also for user-mode access)
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

	// Finalize initialization
	ClearFlag(pDevObj->Flags, DO_DEVICE_INITIALIZING);
	LOG("Driver initialization completed successfully");

	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp) {
	UNREFERENCED_PARAMETER(pDev);

	irp->IoStatus.Information = sizeof(Driver::Info_t);

	// Get the current stack location and the system buffer
	// The stack location contains the IOCTL code and other parameters
	// The buffer contains the data sent from user-mode and where we write data back
	auto stack = IoGetCurrentIrpStackLocation(irp);
	auto buffer = (Driver::Info_t*)irp->AssociatedIrp.SystemBuffer;
	auto ctlCode = stack->Parameters.DeviceIoControl.IoControlCode;

	if (!stack) { // Handle non-existent stack
		LOG("ERROR: Stack is NULL");

		irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);

		return STATUS_INVALID_PARAMETER;
	}

	// Use the requested handler
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	switch (ctlCode) {
	case Codes::READCODE:
		status = HandleReadRequest(buffer);
		break;
	case Codes::WRITECODE:
		status = HandleWriteRequest(buffer);
		break;
	case Codes::INITCODE:
		status = HandleInitRequest(buffer);
		break;
	case Codes::GETBASECODE:
		status = HandleGetBaseRequest(buffer);
		break;
	}

	// Complete the request
	irp->IoStatus.Status = status;
	KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS Driver::HandleInitRequest(Info_t* buffer) {
	if (!buffer) { // Handle null buffer
		LOG("ERROR: Buffer is NULL in INIT request");
		return STATUS_INVALID_PARAMETER;
	}

	// Lookup the target process by its PID, this gives us a PEPROCESS
	NTSTATUS getProcessStatus = KFNs::pPsLookupProcessByProcessId(buffer->processId, &Driver::targetProcess);

	if (!NT_SUCCESS(getProcessStatus)) { // Handle lookup failure
		LOG("ERROR: GetProcessByName failed with status 0x%X", getProcessStatus);
		Driver::targetProcess = nullptr;
		return getProcessStatus;
	}

	return STATUS_SUCCESS;
}

UINT64 Driver::GetPML4Base() {
	constexpr UINT64 DirBaseOffset = 0x28;

	auto dirBase = *reinterpret_cast<ULONG64*>(
		reinterpret_cast<UINT64>(Driver::targetProcess) + DirBaseOffset
	);

	return dirBase;
}

PVOID Driver::TranslateVirtualToPhysical(VirtualAddress va) {
	// Extract indices and offset from the virtual address
	const auto pml4Index = va.parts.pml4_index;
	const auto pdptIndex = va.parts.pdpt_index;
	const auto pdIndex = va.parts.pd_index;
	const auto ptIndex = va.parts.pt_index;
	const auto pageOffset = va.parts.offset;

	// Used for every ReadPhysicalMemory call
	// We simply reuse the same variable
	SIZE_T bytesRead{};

	// The PML4 base address in physical memory
	// We use this to walk the page table hierarchy
	ULONG64 pml4PhysBase = GetPML4Base();

	// First, we read the PML4 entry
	// This will point us to the PDPT table
	// We use `pml4Index` to get the correct entry
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

	// Next, we read the PDPT entry
	// This will point us to the PD table
	// We use `pdptIndex` to get the correct entry
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
	// If this is a 1 GiB page, we can directly calculate the physical address
	// without going further down the page table hierarchy
	if (pdpte.parts.pageSize) {
		const auto physBase = (pdpte.parts.physBase << 12);
		const auto bigPageOffset = va.value & ((1ull << 30) - 1); // lower 30 bits

		return reinterpret_cast<PVOID>(physBase + bigPageOffset);
	}

	// Next, we read the PD entry
	// This will point us to the PT table
	// We use `pdIndex` to get the correct entry
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
	// If this is a 2 MiB page, we can directly calculate the physical address
	// without going further down the page table hierarchy
	if (pde.parts.pageSize) {
		const auto physBase = (pde.parts.physBase << 12);
		const auto largePageOffset = va.value & ((1ull << 21) - 1); // lower 21 bits
		return reinterpret_cast<PVOID>(physBase + largePageOffset);
	}

	// Finally, we read the PT entry
	// This will point us to the actual physical page
	// We use `ptIndex` to get the correct entry
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

	// Calculate the final physical address
	// This is done by combining the physical page base address
	// with the page offset from the original virtual address
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

	// To write, we first map the physical memory into our address space
	// This is because we cannot directly write to physical memory
	VOID* mappedAddress = KFNs::pMmMapIoSpace(physAddr, size, MmNonCached);

	if (!mappedAddress) { // Handle mapping failure
		LOG("ERROR: MmMapIoSpace failed in WRITE request");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Now we can copy the data from our buffer to the mapped physical memory
	RtlCopyMemory(mappedAddress, buffer, size);

	// Unmap the physical memory from our address space
	KFNs::pMmUnmapIoSpace(mappedAddress, size);

	if (bytesWritten) *bytesWritten = size;
	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleReadRequest(Info_t* buffer) {
	if (!buffer) { // Handle null buffer
		LOG("ERROR: Buffer is NULL in READ request");
		return STATUS_INVALID_PARAMETER;
	}

	if (!Driver::targetProcess) { // Handle no target process (user needs to call INIT first)
		LOG("ERROR: No target process attached in READ request");
		return STATUS_INVALID_PARAMETER;
	}

	VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
	VOID* physAddress = TranslateVirtualToPhysical(va);

	// Prevent crossing page boundaries (this would cause issues)
	// TODO: Handle multi-page reads/writes (we just read/write up to the page boundary for now)
	ULONG64 finalSize = Min(PAGE_SIZE - ((INT64)physAddress & 0xFFF), (LONGLONG)buffer->bytesToRead);
    SIZE_T bytesRead = NULL;

	// Read the physical memory into the user buffer
	NTSTATUS res = ReadPhysicalMemory(PVOID(physAddress), buffer->bufferAddress, finalSize, &bytesRead);
	if (!NT_SUCCESS(res)) {
		LOG("ERROR: ReadPhysicalMemory failed in READ request with status 0x%X", res);
		return res;
	}

	buffer->bytesRead = bytesRead;
	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleWriteRequest(Info_t* buffer) {
	if (!buffer) { // Handle null buffer
		LOG("ERROR: Buffer is NULL in WRITE request");
		return STATUS_INVALID_PARAMETER;
	}

	if (!Driver::targetProcess) { // Handle no target process (user needs to call INIT first)
		LOG("ERROR: No target process attached in WRITE request");
		return STATUS_INVALID_PARAMETER;
	}

	SIZE_T bytesWritten = 0;

	VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
	VOID* physAddress = TranslateVirtualToPhysical(va);

	// Prevent crossing page boundaries (this would cause issues)
	// TODO: Handle multi-page reads/writes (we just read/write up to the page boundary for now)
	ULONG64 finalSize = Min(PAGE_SIZE - ((INT64)physAddress & 0xFFF), (LONGLONG)buffer->bytesToRead);
	NTSTATUS res = WritePhysicalMemory(PVOID(physAddress), buffer->bufferAddress, finalSize, &bytesWritten);

	if (!NT_SUCCESS(res)) { // Handle write failure
		LOG("ERROR: WritePhysicalMemory failed in WRITE request with status 0x%X", res);
		return res;
	}

	buffer->bytesRead = bytesWritten;
	return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleGetBaseRequest(Info_t* buffer) {
	if (!buffer) { // Handle null buffer
		LOG("ERROR: Buffer is NULL in GETBASE request");
		return STATUS_INVALID_PARAMETER;
	}

	if (!Driver::targetProcess) { // Handle no target process (user needs to call INIT first)
		LOG("ERROR: No target process attached in GETBASE request");
		return STATUS_INVALID_PARAMETER;
	}

	// EPROCESS+0x2B0 == SectionBaseAddress
	UINT64 baseAddress = *reinterpret_cast<UINT64*>(
		reinterpret_cast<UINT64>(Driver::targetProcess) + 0x2B0
		);

	buffer->targetAddress = reinterpret_cast<PVOID>(baseAddress);
	return STATUS_SUCCESS;
}
