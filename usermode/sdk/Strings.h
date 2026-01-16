#pragma once

#include <string>
#include <optional>

#include "sdk_offsets.h"
#include "sdk_globals.h"
#include "driver.h"

namespace SDK {

    class FNamePool {
    public:
        static FNamePool* Get() {
            uintptr_t address = imgBase + Offsets::GNamePool_Offset;
            return reinterpret_cast<FNamePool*>(address);
        }

        std::optional<std::string> GetNameAsString(uint32_t chunkOffset, uint16_t nameOffset) const {
            uintptr_t pThis = reinterpret_cast<uintptr_t>(this);
            uintptr_t pChunk = dm.read<uintptr_t>(pThis + 8ull * (chunkOffset + 2));
            uintptr_t entryOffset = pChunk + 2ull * nameOffset;

            uint16_t nameLengthRaw = dm.read<uint16_t>(entryOffset);
            size_t nameLength = nameLengthRaw >> 6;

            if (nameLength == 0 || nameLength > 1024) {
                return std::nullopt;
            }

            std::string nameBuffer(nameLength, '\0');
            auto nameBufferOpaque = reinterpret_cast<uint8_t*>(nameBuffer.data());
            dm.read(entryOffset + 2, nameBufferOpaque, nameLength);
            return nameBuffer;
        }
    };

    class FName {
    public:
        FName() : ComparisonIndex(0), Number(0) {}
        FName(uint32_t comparisonIndex, uint32_t number)
			: ComparisonIndex(comparisonIndex), Number(number) {
		}
        FName(uint32_t comparisonIndex)
			: ComparisonIndex(comparisonIndex), Number(0) {
		}

        std::string ToString() const {
            uint32_t chunkOffset = ComparisonIndex >> 16;
            uint16_t nameOffset = static_cast<uint16_t>(ComparisonIndex & 0xFFFF);

            auto nameOpt = SDK::GNamePool->GetNameAsString(chunkOffset, nameOffset);
            if (nameOpt.has_value()) {
                return nameOpt.value();
            }
            return std::string("<invalid name>");
        }

        uint32_t ComparisonIndex;
        uint32_t Number;
    };

    // Owned string data
    class FString {
    public:
        FString() : Data() {}

        std::string ToString() {
            std::string result;
            for (uint16_t ch : Data) {
                if (ch == 0) break; // Null-terminated
                result += static_cast<char>(ch);
            }

            return result;
        }

		const TArray<uint16_t>& GetData() const { return Data; }

    private:
		TArray<uint16_t> Data;
    };
}
