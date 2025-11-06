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

		int16_t mz = dm.read<int16_t>(baseAddress);
		char pe[3] = {};
		memcpy(pe, &mz, sizeof(mz));
		pe[2] = '\0';

		std::println("MZ Signature: {}", pe);

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