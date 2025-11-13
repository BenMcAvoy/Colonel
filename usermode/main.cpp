#include "driver.h"

#include <iostream>
#include <format>
#include <print>

static DriverManager dm("colonelLink");
static uintptr_t imgBase = 0;

namespace Offsets {
	namespace G {
		constexpr uintptr_t contraption = 0x1267538;
	}

	namespace Contraption {
		constexpr uintptr_t width = 0x68;
		constexpr uintptr_t height = 0x6C;
	}
};

class Contraption {
public:
	Contraption(uintptr_t addr) : base(addr) {}

private:
	uintptr_t base;

public:
	uint32_t getWidth() const { return dm.read<uint32_t>(base + Offsets::Contraption::width); }
	uint32_t getHeight() const { return dm.read<uint32_t>(base + Offsets::Contraption::height); }

	static Contraption getInstance() {
		uintptr_t addr = dm.read<uintptr_t>(imgBase + Offsets::G::contraption);
		return Contraption{ addr };
	}
};

int main(void) {
	try {
		dm.attachToProcess("ScrapMechanic.exe", &imgBase);

		auto contraption = Contraption::getInstance();
		auto width = contraption.getWidth();
		auto height = contraption.getHeight();

		std::println("Contraption dimensions: {} x {}", width, height);

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