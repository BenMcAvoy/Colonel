#include "driver.h"

#include <iostream>
#include <format>
#include <print>

static DriverManager dm("colonelLink");
static uintptr_t imgBase = 0;

int main(void) {
	try {
		dm.attachToProcess("ScrapMechanic.exe", false, &imgBase);

		std::println("Base address: 0x{:X}", imgBase);

		int32_t magic = dm.read<int32_t>(imgBase);
		std::println("Magic value at base address: 0x{:X}", magic);

		return 0;
	}
	catch (const DriverException& e) {
		std::cerr << "Driver error: " << e.what() << std::endl;
		std::cerr << "Status: " << DriverException::getStatusMessage(e.status()) << std::endl;
		return -1;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}
}