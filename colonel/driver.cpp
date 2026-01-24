#include "driver.h"

#include <vendor/skCrypter.h>
#include <vendor/kli.hpp>

using namespace Driver;

#define GAF_ASYNC_KEYSTATE_RVA  0x24F8C0
#define KEYSTATE_ARRAY_SIZE     0x400

static PHYSICAL_ADDRESS g_KeyStatePhysAddr = {0};
static BOOLEAN g_KeyStateCached = FALSE;

typedef struct _KLDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} KLDR_DATA_TABLE_ENTRY, *PKLDR_DATA_TABLE_ENTRY;

#define INITIALIZE(fn) do { \
    p##fn = KLI_FN(fn); \
    LOG("KLI Cache - %s initialized at %p", __FUNCTION__, #fn, p##fn); \
} while(0)

NTSTATUS GetKernelModuleBase(
    _In_ PCWSTR ModuleName,
    _Out_ PVOID* OutBase,
    _Out_opt_ PSIZE_T OutSize
)
{
    if (!OutBase) return STATUS_INVALID_PARAMETER;

    *OutBase = nullptr;
    if (OutSize) *OutSize = 0;

    if (!KFNs::pPsLoadedModuleList)
    {
        LOG("PsLoadedModuleList not resolved");
        return STATUS_NOT_FOUND;
    }

    UNICODE_STRING targetName;
    RtlInitUnicodeString(&targetName, ModuleName);

    PLIST_ENTRY current = KFNs::pPsLoadedModuleList->Flink;

    while (current != KFNs::pPsLoadedModuleList)
    {
        PKLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(
            current,
            KLDR_DATA_TABLE_ENTRY,
            InLoadOrderLinks
        );

        if (RtlEqualUnicodeString(&entry->BaseDllName, &targetName, TRUE))
        {
            *OutBase = entry->DllBase;
            if (OutSize) *OutSize = entry->SizeOfImage;
            LOG("Found module %ws at %p (size 0x%X)", ModuleName, *OutBase, entry->SizeOfImage);
            return STATUS_SUCCESS;
        }

        current = current->Flink;
    }

    LOG("Module %ws not found in PsLoadedModuleList", ModuleName);
    return STATUS_NOT_FOUND;
}

typedef struct _IMAGE_NT_HEADERS64
{
    ULONG Signature;
    kli::detail::IMAGE_FILE_HEADER FileHeader;
    kli::detail::IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;

NTSTATUS FindExportByName(
    _In_ PVOID ModuleBase,
    _In_ const char* ExportName, // e.g. "_GetAsyncKeyState"
    _Out_ PVOID* OutFunctionAddress
)
{
    *OutFunctionAddress = nullptr;

    if (!ModuleBase || !ExportName) return STATUS_INVALID_PARAMETER;

    kli::detail::PIMAGE_DOS_HEADER dos = static_cast<kli::detail::PIMAGE_DOS_HEADER>(ModuleBase);
    if (dos->e_magic != kli::detail::IMAGE_DOS_SIGNATURE)
    {
        LOG("Invalid DOS header");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(static_cast<PUCHAR>(ModuleBase) + dos->e_lfanew);
    if (nt->Signature != kli::detail::IMAGE_NT_SIGNATURE)
    {
        LOG("Invalid NT header");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    kli::detail::PIMAGE_DATA_DIRECTORY expDir = &nt->OptionalHeader.DataDirectory[
        kli::detail::IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir->VirtualAddress == 0 || expDir->Size == 0)
    {
        LOG("Module has no export directory");
        return STATUS_NOT_FOUND;
    }

    kli::detail::PIMAGE_EXPORT_DIRECTORY expDesc = (kli::detail::PIMAGE_EXPORT_DIRECTORY)(
        static_cast<PUCHAR>(ModuleBase) + expDir->VirtualAddress
    );

    if (expDesc->NumberOfNames == 0 || expDesc->AddressOfNames == 0)
    {
        LOG("No export names present");
        return STATUS_NOT_FOUND;
    }

    ULONG* nameRvas = (ULONG*)(static_cast<PUCHAR>(ModuleBase) + expDesc->AddressOfNames);
    USHORT* ordinals = (USHORT*)(static_cast<PUCHAR>(ModuleBase) + expDesc->AddressOfNameOrdinals);
    ULONG* functions = (ULONG*)(static_cast<PUCHAR>(ModuleBase) + expDesc->AddressOfFunctions);

    for (ULONG i = 0; i < expDesc->NumberOfNames; i++)
    {
        const char* name = (const char*)(static_cast<PUCHAR>(ModuleBase) + nameRvas[i]);

        if (strcmp(name, ExportName) == 0)
        {
            USHORT ordinal = ordinals[i];
            ULONG rva = functions[ordinal];

            *OutFunctionAddress = static_cast<PVOID>((PUCHAR)ModuleBase + rva);
            LOG("Found export '%s' → ordinal %hu, RVA 0x%X, VA %p",
                ExportName, ordinal, rva, *OutFunctionAddress);
            return STATUS_SUCCESS;
        }
    }

    LOG("Export '%s' not found in export table", ExportName);
    return STATUS_NOT_FOUND;
}

void KFNs::Initialize()
{
    INITIALIZE(DbgPrintEx);
    INITIALIZE(IoCreateDevice);
    INITIALIZE(IoCreateSymbolicLink);
    INITIALIZE(IofCompleteRequest);
    INITIALIZE(PsLookupProcessByProcessId);
    INITIALIZE(MmCopyMemory);
    INITIALIZE(MmMapIoSpace);
    INITIALIZE(MmUnmapIoSpace);
    INITIALIZE(IoCreateDriver);
    INITIALIZE(RtlInitUnicodeString);

    UNICODE_STRING listName;
    RtlInitUnicodeString(&listName, L"PsLoadedModuleList");
    pPsLoadedModuleList = reinterpret_cast<PLIST_ENTRY>(
        MmGetSystemRoutineAddress(&listName)
    );

    if (pPsLoadedModuleList)
        LOG("PsLoadedModuleList resolved → %p", pPsLoadedModuleList);
    else
        LOG("Failed to resolve PsLoadedModuleList");

    PVOID win32kBase = nullptr;
    SIZE_T modSize = 0;

    NTSTATUS status = GetKernelModuleBase(L"win32kbase.sys", &win32kBase, &modSize);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to find win32kbase.sys");
        return;
    }

    LOG("win32kbase.sys base: %p  size: 0x%zx", win32kBase, modSize);

    PVOID funcAddr = nullptr;
    status = FindExportByName(win32kBase, "_GetAsyncKeyState", &funcAddr);

    if (NT_SUCCESS(status) && funcAddr)
    {
        pGetAsyncKeyState = static_cast<GetAsyncKeyState_t>(funcAddr);
        LOG("_GetAsyncKeyState found in export table at %p", pGetAsyncKeyState);
    }
    else LOG("Could not find '_GetAsyncKeyState' in export table (status 0x%X)", status);
}

NTSTATUS Driver::HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp)
{
    UNREFERENCED_PARAMETER(pDeviceObj);
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp)
{
    UNREFERENCED_PARAMETER(pDeviceObj);
    LOG("CreateIO called. Device opened");
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

NTSTATUS Driver::HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp)
{
    UNREFERENCED_PARAMETER(pDeviceObj);
    LOG("CloseIO called. Device closed");
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

NTSTATUS NTAPI Driver::MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath)
{
    UNREFERENCED_PARAMETER(regPath);

    PDEVICE_OBJECT pDevObj = nullptr;

    auto devName = INIT_USTRING(L"\\Device\\colonelDevice");
    auto symLink = INIT_USTRING(L"\\??\\colonelLink");
    auto symLinkGlobal = INIT_USTRING(L"\\DosDevices\\Global\\colonelLink");

    LOG("Creating device: \\Device\\colonelDevice");
    auto status = KFNs::pIoCreateDevice(pDriver, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE,
                                        &pDevObj);
    if (!NT_SUCCESS(status))
    {
        LOG("ERROR: IoCreateDevice failed with status 0x%X", status);
        return status;
    }

    LOG("Creating symbolic link: \\??\\colonelLink");
    status = KFNs::pIoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status))
    {
        LOG("ERROR: IoCreateSymbolicLink(\\??) failed with status 0x%X", status);
        return status;
    }

    LOG("Creating symbolic link: \\DosDevices\\Global\\colonelLink");
    NTSTATUS statusGlobal = KFNs::pIoCreateSymbolicLink(&symLinkGlobal, &devName);
    if (!NT_SUCCESS(statusGlobal))
    {
        LOG("ERROR: IoCreateSymbolicLink(\\DosDevices\\Global) failed with status 0x%X", statusGlobal);
        return statusGlobal;
    }

    SetFlag(pDevObj->Flags, DO_BUFFERED_IO);

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        pDriver->MajorFunction[i] = Driver::HandleUnsupportedIO;

    pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = Driver::HandleIORequest;
    pDriver->MajorFunction[IRP_MJ_CREATE] = Driver::HandleCreateIO;
    pDriver->MajorFunction[IRP_MJ_CLOSE] = Driver::HandleCloseIO;
    pDriver->DriverUnload = NULL;

    ClearFlag(pDevObj->Flags, DO_DEVICE_INITIALIZING);
    LOG("Driver initialization completed successfully");

    return STATUS_SUCCESS;
}

NTSTATUS Driver::ReadKeyState(ULONG vkCode, PBOOLEAN isDown)
{
    *isDown = FALSE;

    if (vkCode >= 0x100) return STATUS_INVALID_PARAMETER;

    if (!targetProcess)
    {
        LOG("No target process attached");
        return STATUS_NOT_FOUND;
    }

    SHORT state = KFNs::pGetAsyncKeyState(vkCode);
    *isDown = (state & 0x8000) != 0;

    LOG("_GetAsyncKeyState(0x%X) → 0x%04X (pressed = %d)", vkCode, state, *isDown);

    return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp)
{
    UNREFERENCED_PARAMETER(pDev);

    irp->IoStatus.Information = sizeof(Driver::Info_t);

    auto stack = IoGetCurrentIrpStackLocation(irp);
    auto buffer = static_cast<Driver::Info_t*>(irp->AssociatedIrp.SystemBuffer);
    auto ctlCode = stack->Parameters.DeviceIoControl.IoControlCode;

    if (!stack)
    {
        LOG("ERROR: Stack is NULL");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    switch (ctlCode)
    {
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
    case Codes::KEYSTATECODE:
        if (buffer && buffer->key <= 0xFF)
        {
            LOG("HandleIORequest: KEYSTATECODE request for VK 0x%X", buffer->key);
            BOOLEAN down = FALSE;
            status = ReadKeyState(buffer->key, &down);
            if (NT_SUCCESS(status))
            {
                buffer->isDown = down;
            }
            else
            {
                LOG("HandleIORequest: KEYSTATECODE failed - status 0x%X", status);
            }
        }
        else
        {
            LOG("HandleIORequest: KEYSTATECODE invalid parameter (key = 0x%X)",
                buffer ? buffer->key : 0);
            status = STATUS_INVALID_PARAMETER;
        }
        break;
    }

    irp->IoStatus.Status = status;
    KFNs::pIofCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS Driver::HandleInitRequest(Info_t* buffer)
{
    if (!buffer)
    {
        LOG("ERROR: Buffer is NULL in INIT request");
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS getProcessStatus = KFNs::pPsLookupProcessByProcessId(buffer->processId, &Driver::targetProcess);

    if (!NT_SUCCESS(getProcessStatus))
    {
        LOG("ERROR: PsLookupProcessByProcessId failed with status 0x%X", getProcessStatus);
        Driver::targetProcess = nullptr;
        return getProcessStatus;
    }

    LOG("Successfully set target process to PID %llu", buffer->processId);

    return STATUS_SUCCESS;
}

UINT64 Driver::GetPML4Base()
{
    constexpr UINT64 DirBaseOffset = 0x28;

    auto dirBase = *reinterpret_cast<ULONG64*>(
        reinterpret_cast<UINT64>(Driver::targetProcess) + DirBaseOffset
    );

    return dirBase;
}

PVOID Driver::TranslateVirtualToPhysical(VirtualAddress va)
{
    const auto pml4Index = va.parts.pml4_index;
    const auto pdptIndex = va.parts.pdpt_index;
    const auto pdIndex = va.parts.pd_index;
    const auto ptIndex = va.parts.pt_index;
    const auto pageOffset = va.parts.offset;

    SIZE_T bytesRead{};

    ULONG64 pml4PhysBase = GetPML4Base();

    PTE pml4e{};
    {
        NTSTATUS st = ReadPhysicalMemory(
            reinterpret_cast<PVOID>(pml4PhysBase + pml4Index * sizeof(UINT64)),
            &pml4e,
            sizeof(UINT64),
            &bytesRead);

        if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pml4e.parts.present)
        {
            LOG("PML4 entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
            return nullptr;
        }
    }

    PTE pdpte{};
    {
        NTSTATUS st = ReadPhysicalMemory(
            reinterpret_cast<PVOID>((pml4e.parts.physBase << 12) + pdptIndex * sizeof(UINT64)),
            &pdpte,
            sizeof(UINT64),
            &bytesRead);

        if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pdpte.parts.present)
        {
            LOG("PDPT entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
            return nullptr;
        }
    }

    if (pdpte.parts.pageSize)
    {
        const auto physBase = pdpte.parts.physBase << 12;
        const auto bigPageOffset = va.value & (1ull << 30) - 1;
        return reinterpret_cast<PVOID>(physBase + bigPageOffset);
    }

    PTE pde{};
    {
        NTSTATUS st = ReadPhysicalMemory(
            reinterpret_cast<PVOID>((pdpte.parts.physBase << 12) + pdIndex * sizeof(UINT64)),
            &pde,
            sizeof(UINT64),
            &bytesRead);

        if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pde.parts.present)
        {
            LOG("PD entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
            return nullptr;
        }
    }

    if (pde.parts.pageSize)
    {
        const auto physBase = pde.parts.physBase << 12;
        const auto largePageOffset = va.value & (1ull << 21) - 1;
        return reinterpret_cast<PVOID>(physBase + largePageOffset);
    }

    PTE pte{};
    {
        NTSTATUS st = ReadPhysicalMemory(
            reinterpret_cast<PVOID>((pde.parts.physBase << 12) + ptIndex * sizeof(UINT64)),
            &pte,
            sizeof(UINT64),
            &bytesRead);

        if (!NT_SUCCESS(st) || bytesRead != sizeof(UINT64) || !pte.parts.present)
        {
            LOG("PT entry not present or read failed (st=0x%X, bytes=%Iu)", st, bytesRead);
            return nullptr;
        }
    }

    const auto phys = (pte.parts.physBase << 12) + pageOffset;
    return reinterpret_cast<PVOID>(phys);
}

NTSTATUS Driver::ReadPhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead)
{
    *bytesRead = 0;

    if (size > PAGE_SIZE) return STATUS_BUFFER_TOO_SMALL;

    UCHAR safeBuffer[PAGE_SIZE];

    MM_COPY_ADDRESS fromAddress;
    fromAddress.PhysicalAddress.QuadPart = reinterpret_cast<UINT64>(physicalAddress);

    SIZE_T copied = 0;
    NTSTATUS status = KFNs::pMmCopyMemory(safeBuffer, fromAddress, size, MM_COPY_MEMORY_PHYSICAL, &copied);

    if (NT_SUCCESS(status) && copied > 0)
    {
        RtlCopyMemory(buffer, safeBuffer, copied);
        *bytesRead = copied;
    }

    return status;
}

NTSTATUS Driver::WritePhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten)
{
    PHYSICAL_ADDRESS physAddr{};
    physAddr.QuadPart = reinterpret_cast<UINT64>(physicalAddress);

    VOID* mappedAddress = KFNs::pMmMapIoSpace(physAddr, size, MmNonCached);

    if (!mappedAddress)
    {
        LOG("ERROR: MmMapIoSpace failed in WRITE request");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(mappedAddress, buffer, size);

    KFNs::pMmUnmapIoSpace(mappedAddress, size);

    if (bytesWritten) *bytesWritten = size;
    return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleReadRequest(Info_t* buffer)
{
    if (!buffer)
    {
        LOG("ERROR: Buffer is NULL in READ request");
        return STATUS_INVALID_PARAMETER;
    }

    if (!Driver::targetProcess)
    {
        LOG("ERROR: No target process attached in READ request");
        return STATUS_INVALID_PARAMETER;
    }

    VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
    SIZE_T remaining = buffer->bytesToRead;
    SIZE_T totalRead = 0;
    UINT8* userBuffer = reinterpret_cast<UINT8*>(buffer->bufferAddress);
    UINT64 currentVA = va.value;

    while (remaining > 0)
    {
        VOID* physAddress = TranslateVirtualToPhysical(*reinterpret_cast<VirtualAddress*>(&currentVA));
        if (!physAddress)
        {
            LOG("ERROR: Failed to translate VA 0x%llX", currentVA);
            break;
        }

        SIZE_T offsetInPage = (UINT64)physAddress & 0xFFF;
        SIZE_T chunkSize = min(PAGE_SIZE - offsetInPage, remaining);
        SIZE_T bytesRead = 0;

        NTSTATUS res = ReadPhysicalMemory(physAddress, userBuffer, chunkSize, &bytesRead);
        if (!NT_SUCCESS(res) || bytesRead == 0)
        {
            LOG("ERROR: ReadPhysicalMemory failed at VA 0x%llX with status 0x%X", currentVA, res);
            return res;
        }

        userBuffer += bytesRead;
        currentVA += bytesRead;
        remaining -= bytesRead;
        totalRead += bytesRead;
    }

    buffer->bytesRead = totalRead;
    return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleWriteRequest(Info_t* buffer)
{
    if (!buffer)
    {
        LOG("ERROR: Buffer is NULL in WRITE request");
        return STATUS_INVALID_PARAMETER;
    }

    if (!Driver::targetProcess)
    {
        LOG("ERROR: No target process attached in WRITE request");
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T bytesWritten = 0;

    VirtualAddress& va = *reinterpret_cast<VirtualAddress*>(&buffer->targetAddress);
    VOID* physAddress = TranslateVirtualToPhysical(va);

    LOG("WRITE VA 0x%llX at PA %p", va.value, physAddress);

    ULONG64 finalSize = Min(PAGE_SIZE - ((INT64)physAddress & 0xFFF), static_cast<LONGLONG>(buffer->bytesToRead));

    if (finalSize != buffer->bytesToRead)
    {
        LOG("WARNING: Write request crosses page boundary, limiting write size to %llu bytes", finalSize);
    }

    NTSTATUS res = WritePhysicalMemory(static_cast<PVOID>(physAddress), buffer->bufferAddress, finalSize,
                                       &bytesWritten);

    if (!NT_SUCCESS(res))
    {
        LOG("ERROR: WritePhysicalMemory failed in WRITE request with status 0x%X", res);
        return res;
    }

    buffer->bytesRead = bytesWritten;
    return STATUS_SUCCESS;
}

NTSTATUS Driver::HandleGetBaseRequest(Info_t* buffer)
{
    if (!buffer)
    {
        LOG("ERROR: Buffer is NULL in GETBASE request");
        return STATUS_INVALID_PARAMETER;
    }

    if (!Driver::targetProcess)
    {
        LOG("ERROR: No target process attached in GETBASE request");
        return STATUS_INVALID_PARAMETER;
    }

    UINT64 baseAddress = *reinterpret_cast<UINT64*>(
        reinterpret_cast<UINT64>(Driver::targetProcess) + 0x2B0
    );

    buffer->targetAddress = reinterpret_cast<PVOID>(baseAddress);
    return STATUS_SUCCESS;
}
