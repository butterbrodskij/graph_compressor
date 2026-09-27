#pragma once

#include <vector>

#include <cstddef>
#include <cstdint>

namespace NGraphCompressor {

using TBytes = std::vector<uint8_t>;

class TBitWriter {
public:
    void Write(uint64_t value, unsigned count);
    void Zeros(uint64_t count);
    void Rice(uint64_t value, unsigned parameter);
    // Truncated binary coding of a value in [0, range).
    void Uniform(uint64_t value, uint64_t range);
    TBytes Take();

private:
    TBytes Data;
    unsigned Used = 0;
};

class TBitReader {
public:
    explicit TBitReader(const TBytes& data);

    uint64_t Read(unsigned count);
    uint64_t Rice(unsigned parameter);
    uint64_t Uniform(uint64_t range);
    unsigned ArithmeticBit();

private:
    const TBytes& Data;
    uint64_t Position = 0;
};

// Choose the exact minimum over all parameters, including byte padding.
unsigned ChooseRice(const std::vector<uint32_t>& values);
TBytes EncodeRice(const std::vector<uint32_t>& values, unsigned parameter);

} // namespace NGraphCompressor
