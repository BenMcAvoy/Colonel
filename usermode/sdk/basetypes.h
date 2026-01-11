#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <format>

#include "sdk_globals.h"

namespace SDK {
    struct FQuat {
        float X;
        float Y;
        float Z;
        float W;
        FQuat() : X(0), Y(0), Z(0), W(1) {}
        FQuat(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
        std::string ToString() const {
            return std::format("FQuat(X: {}, Y: {}, Z: {}, W: {})", X, Y, Z, W);
        }
	};

    struct FVector {
        float X;
        float Y;
        float Z;

        FVector() : X(0), Y(0), Z(0) {}
        FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}

        FVector operator+(const FVector& other) const {
            return FVector(X + other.X, Y + other.Y, Z + other.Z);
        }
        FVector operator-(const FVector& other) const {
            return FVector(X - other.X, Y - other.Y, Z - other.Z);
        }
        FVector operator*(float scalar) const {
            return FVector(X * scalar, Y * scalar, Z * scalar);
        }
        FVector operator/(float scalar) const {
            return FVector(X / scalar, Y / scalar, Z / scalar);
        }

        float Size() const {
            return std::sqrt(X * X + Y * Y + Z * Z);
		}

        float Dot(FVector Other) {
            return X * Other.X + Y * Other.Y + Z * Other.Z;
        }

        std::string ToString() const {
            return std::format("FVector(X: {}, Y: {}, Z: {})", X, Y, Z);
        }
    };

    struct FRotator {
        float Pitch;
        float Yaw;
        float Roll;

        FRotator() : Pitch(0), Yaw(0), Roll(0) {}
        FRotator(float pitch, float yaw, float roll) : Pitch(pitch), Yaw(yaw), Roll(roll) {}

        std::string ToString() const {
            return std::format("FRotator(Pitch: {}, Yaw: {}, Roll: {})", Pitch, Yaw, Roll);
        }
	};

    struct FMatrix {
        float M[4][4];

        FMatrix() {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    M[i][j] = 0.0f;
                }
            }
        }

        FMatrix operator*(const FMatrix& Other) const {
            FMatrix Result;

            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    Result.M[row][col] =
                        M[row][0] * Other.M[0][col] +
                        M[row][1] * Other.M[1][col] +
                        M[row][2] * Other.M[2][col] +
                        M[row][3] * Other.M[3][col];
                }
            }

            return Result;
        }

        std::string ToString() const {
            std::string result = "FMatrix(\n";
            for (int i = 0; i < 4; ++i) {
                result += "  [";
                for (int j = 0; j < 4; ++j) {
                    result += std::format("{:>8.3f}", M[i][j]);
                    if (j < 3) result += ", ";
                }
                result += "]\n";
            }
            result += ")";
            return result;
        }

        explicit FMatrix(class FRotator& Rot) {
            const float RadPitch = Rot.Pitch * 3.14159265f / 180.0f;
            const float RadYaw = Rot.Yaw * 3.14159265f / 180.0f;
            const float RadRoll = Rot.Roll * 3.14159265 / 180.0f;

            const float SP = std::sin(RadPitch);
            const float CP = std::cos(RadPitch);
            const float SY = std::sin(RadYaw);
            const float CY = std::cos(RadYaw);
            const float SR = std::sin(RadRoll);
            const float CR = std::cos(RadRoll);

            M[0][0] = CP * CY;
            M[0][1] = CP * SY;
            M[0][2] = SP;
            M[0][3] = 0.0f;

            M[1][0] = SR * SP * CY - CR * SY;
            M[1][1] = SR * SP * SY + CR * CY;
            M[1][2] = -SR * CP;
            M[1][3] = 0.0f;

            M[2][0] = -(CR * SP * CY + SR * SY);
            M[2][1] = CY * SR - CR * SP * SY;
            M[2][2] = CR * CP;
            M[2][3] = 0.0f;

            M[3][0] = 0.0f; // origin.roll
            M[3][1] = 0.0f; // origin.yaw
            M[3][2] = 0.0f; // origin.pitch
            M[3][3] = 1.0f;
        }
    };

    template <typename T>
    class TArray {
    public:
        TArray() : Data(nullptr), Count(0), Max(0) {}
        TArray(T* data, int32_t count, int32_t max) : Data(data), Count(count), Max(max) {}
		TArray(T* data, int32_t count) : Data(data), Count(count), Max(count) {}
		TArray(T* data, size_t count) : Data(data), Count(count), Max(count) {}
		TArray(const TArray& other) : Data(other.Data), Count(other.Count), Max(other.Max) {}

        T operator[](int32_t index) const {
            if (index < 0 || index >= Count) {
                throw std::out_of_range("TArray index out of range");
            }

			return SDK::dm.read<T>(Data + index);
        }

        struct Iterator {
            T* Current;
            int32_t Index;

            Iterator(T* ptr, int32_t index) : Current(ptr), Index(index) {}

            T operator*() { 
                return SDK::dm.read<T>(reinterpret_cast<uintptr_t>(Current));
            }

            Iterator& operator++() { 
                ++Current; 
                ++Index;
                return *this; 
            }

            bool operator!=(const Iterator& other) const { 
                return Current != other.Current; 
            }
        };

        Iterator begin() { return Iterator(Data, 0); }
        Iterator end() { return Iterator(Data + Count, Count); }

        std::string ToString() const {
            return std::format("TArray(Count: {}, Max: {})", Count, Max);
		}

        T* Data;
        int32_t Count;
        int32_t Max;
    };
}
