#include "driver.h"

#include <iostream>
#include <format>
#include <print>

int main(void) {
	try {
		DriverManager dm("colonelLink");

		uintptr_t baseAddress = 0;
		dm.attachToProcess("BrickRigsSteam-Win64-Shipping.exe", false, &baseAddress);
		std::println("Base address: 0x{:X}", baseAddress);

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