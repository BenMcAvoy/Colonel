#include "driver.h"

#include <iostream>
#include <format>
#include <print>

int main(void) {
	try {
		DriverManager dm("colonelLink");

		uintptr_t baseAddress = 0;
		dm.attachToProcess("notepad.exe", &baseAddress);

		std::println("Base address of notepad.exe: 0x{:X}", baseAddress);

		int16_t iMagic = dm.read<int16_t>(baseAddress);
		char magic[3] = {};
		memcpy(magic, &iMagic, sizeof(iMagic));
		magic[2] = '\0';

		std::println("PE Signature: {}", magic);

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