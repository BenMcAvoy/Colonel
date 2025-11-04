#include "driver.h"

#include <iostream>
#include <format>
#include <print>

int main(void) {
	try {
		DriverManager dm("colonelLink");

		uintptr_t baseAddress = 0;
		dm.attachToProcess("notepad.exe", &baseAddress);

		if (!dm.isValid()) {
			std::fprintf(stderr, "Failed to initialize driver manager.\n");
			return -1;
		}

		printf("Attached to notepad.exe at base address: 0x%p\n", reinterpret_cast<void*>(baseAddress));

		uint16_t data = dm.read<uint16_t>(baseAddress);
		printf("%c%c\n", data & 0xFF, (data >> 8) & 0xFF);

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