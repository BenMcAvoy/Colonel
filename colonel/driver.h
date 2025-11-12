#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <intrin.h>

#include <vendor/kli.hpp>

#include "helpers.h"

/// Define to enable debug logging
/// If undefined, logging calls will not be included in the build
#define COLONEL_DEBUG

extern "C" NTSTATUS NTAPI IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);

/**
 * Namespace to hold cached kernel function pointers.
 * These can be initialized using `KFNs::Initialize()` and used throughout the driver code.
*/
namespace KFNs {
	inline decltype(&IoCreateDevice) pIoCreateDevice = nullptr;
	inline decltype(&IoCreateSymbolicLink) pIoCreateSymbolicLink = nullptr;
	inline decltype(&IoGetCurrentIrpStackLocation) pIoGetCurrentIrpStackLocation = nullptr;
	inline decltype(&IofCompleteRequest) pIofCompleteRequest = nullptr;
	inline decltype(&PsLookupProcessByProcessId) pPsLookupProcessByProcessId = nullptr;
	inline decltype(&MmCopyMemory) pMmCopyMemory = nullptr;
	inline decltype(&MmMapIoSpace) pMmMapIoSpace = nullptr;
	inline decltype(&IoCreateDriver) pIoCreateDriver = nullptr;
	inline decltype(&MmUnmapIoSpace) pMmUnmapIoSpace = nullptr;
	inline decltype(&DbgPrintEx) pDbgPrintEx = nullptr;
	inline decltype(&RtlInitUnicodeString) pRtlInitUnicodeString = nullptr;

	/**
	 * Initializes the cached kernel function pointers.
	 * Call once at passive level during driver initialization.
	*/
	void Initialize();
}

#ifdef COLONEL_DEBUG
#define LOG(fmt, ...) \
    KFNs::pDbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
               "[COLONEL] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

union VirtualAddress;

/// Namespace containing the main driver logic and IOCTL codes.
namespace Driver {
	/// Namespace for constant IOCTL codes used by the driver.
	namespace Codes {
		constexpr ULONG INITCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x775, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // attach

		constexpr ULONG READCODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x776, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // read
		constexpr ULONG WRITECODE = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x777, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); // write
	}

	/**
	 * Structure representing information for memory operations.
	 * This structure is used to pass parameters for reading and writing memory.
	 * It must be kept in sync with the user-mode counterpart.
	 */
	struct Info_t {
		_In_    HANDLE processId;
		_In_    PVOID targetAddress;
		_In_    PVOID bufferAddress;
		_In_    SIZE_T bytesToRead;
		_Out_   SIZE_T bytesRead;
	};

	/**
	 * @brief Main entry point for the driver.
	 * 
	 * This function initializes the driver, creates device objects,
	 * and sets up necessary dispatch routines.
	 * 
	 * @param pDriver Pointer to the driver object.
	 * @param regPath Pointer to the registry path.
	 * @return NTSTATUS code indicating success or failure.
	 * 
	 * @note This function is called by the custom driver entry mechanism
	 * due to manual mapped nature of the driver.
	 */
	NTSTATUS NTAPI MainEntryPoint(PDRIVER_OBJECT pDriver, PUNICODE_STRING regPath);

	/**
	 * @brief Handles unsupported IO requests.
	 * 
	 * This function is called for any IOCTLs or IRP major functions
	 * which are not explicitly supported by the driver.
	 * 
	 * @param pDeviceObj Pointer to the device object.
	 * @param irp Pointer to the I/O request packet.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleUnsupportedIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);

	/**
	 * @brief Handles create IO requests.
	 * 
	 * @param pDeviceObj Pointer to the device object.
	 * @param irp Pointer to the I/O request packet.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleCreateIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);

	/**
	 * @brief Handles close IO requests.
	 * 
	 * @param pDeviceObj Pointer to the device object.
	 * @param irp Pointer to the I/O request packet.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleCloseIO(PDEVICE_OBJECT pDeviceObj, PIRP irp);

	/**
	 * @brief Handles IOCTL requests.
	 * 
	 * @param pDev Pointer to the device object.
	 * @param irp Pointer to the I/O request packet.
	 * @return NTSTATUS code indicating success or failure.
	 * 
	 * @note This function dispatches to specific handlers based on the IOCTL code.
	 */
	NTSTATUS HandleIORequest(PDEVICE_OBJECT pDev, PIRP irp);

	/**
	 * @brief Handles the INIT request to set the target process.
	 * 
	 * This function looks up the target process by its PID,
	 * the PID must be provided in the Info_t structure from user-mode.
	 * 
	 * @param buffer Pointer to the Info_t structure containing the process ID.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleInitRequest(Info_t* buffer);

	/**
	 * @brief Handles the READ request to read memory from the target process.
	 * 
	 * @param buffer Pointer to the Info_t structure containing read parameters.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleReadRequest(Info_t* buffer);

	/**
	 * @brief Handles the WRITE request to write memory to the target process.
	 * 
	 * @param buffer Pointer to the Info_t structure containing write parameters.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS HandleWriteRequest(Info_t* buffer);

	/**
	 * @brief Retrieves the PML4 base address for the specified process.
	 * 
	 * @note Uses `Driver::targetProcess` to get the PML4 base of the target process.
	 * 
	 * @note You can find the KPRocess::DirectoryTableBase offset [here](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/ntos/ke/kprocess/index.htm?tx=158&ts=0,191).
	 * 
	 * @return UINT64 The PML4 base address.
	 */
	UINT64 GetPML4Base();

	/**
	 * @brief Translates a virtual address to its corresponding physical address.
	 * 
	 * @param virtualAddress The virtual address to translate.
	 * @return PVOID The physical address corresponding to the virtual address, or nullptr on failure.
	 */
	PVOID TranslateVirtualToPhysical(VirtualAddress virtualAddress);

	/**
	 * @brief Reads physical memory from the specified physical address.
	 * 
	 * This uses MmCopyMemory under the hood to perform the read operation.
	 * 
	 * @param physicalAddress The physical address to read from.
	 * @param buffer The buffer to store the read data.
	 * @param size The number of bytes to read.
	 * @param bytesRead Pointer to store the number of bytes actually read.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS ReadPhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);

	/**
	 * @brief Writes physical memory to the specified physical address.
	 * 
	 * This maps the physical memory into the driver's address space,
	 * performs the write, and then unmaps it.
	 * 
	 * @param physicalAddress The physical address to write to.
	 * @param buffer The buffer containing the data to write.
	 * @param size The number of bytes to write.
	 * @param bytesWritten Pointer to store the number of bytes actually written.
	 * @return NTSTATUS code indicating success or failure.
	 */
	NTSTATUS WritePhysicalMemory(PVOID physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten);

	static PEPROCESS targetProcess = nullptr;
} // namespace Driver

union PTE {
    UINT64 value;

    struct {
        UINT64 present          : 1;  // bit 0
        UINT64 readWrite        : 1;  // bit 1
        UINT64 userSupervisor   : 1;  // bit 2
        UINT64 pageWriteThru    : 1;  // bit 3
        UINT64 pageCacheDisable : 1;  // bit 4
        UINT64 accessed         : 1;  // bit 5
        UINT64 dirty            : 1;  // bit 6 (only for PDEs pointing to large page)
        UINT64 pageSize         : 1;  // bit 7: 0 = points to page table, 1 = 2MB page
        UINT64 global           : 1;  // bit 8 (ignored for PDE pointing to PT)
        UINT64 available        : 3;  // bits 9-11, OS-specific
        UINT64 physBase         : 40; // bits 12-51: PFN
        UINT64 reserved         : 11; // bits 52-62
        UINT64 executeDisable   : 1;  // bit 63
    } parts;
};

/**
 * Represents a 64-bit virtual address and its components in the x86-64 architecture.
 * This union allows easy access to the different parts of the virtual address (PML4, PDPT, PD, PT, and offset).
 */
union VirtualAddress {
	UINT64 value; // Full 64-bit virtual address

	struct {
		UINT64 offset     : 12; // Page offset (4 KB pages)
		UINT64 pt_index   : 9;  // Page Table
		UINT64 pd_index   : 9;  // Page Directory
		UINT64 pdpt_index : 9;  // Page Directory Pointer Table
		UINT64 pml4_index : 9;  // PML4
		UINT64 reserved   : 16; // Sign-extended bits for canonical addresses
	} parts;
};
