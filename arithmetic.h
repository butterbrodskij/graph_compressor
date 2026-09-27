#pragma once

#include "bit_io.h"

#include <vector>

#include <cstddef>
#include <cstdint>

namespace NGraphCompressor {

// Integer arithmetic coding. Model totals are limited to 2^30 so that every
// positive-frequency symbol retains a nonempty interval after normalization.
class TArithmeticEncoder {
public:
    void Encode(uint64_t cumulative, uint64_t frequency, uint64_t total);
    TBytes Finish();

private:
    void Emit(unsigned bit);

private:
    uint64_t Low = 0;
    uint64_t High = (uint64_t{1} << 32) - 1;
    uint64_t Pending = 0;
    TBitWriter Writer;
};

class TArithmeticDecoder {
public:
    explicit TArithmeticDecoder(const TBytes& data);

    uint64_t Position(uint64_t total) const;
    void Advance(uint64_t cumulative, uint64_t frequency, uint64_t total);

private:
    uint64_t Low = 0;
    uint64_t High = (uint64_t{1} << 32) - 1;
    uint64_t Code = 0;
    TBitReader Reader;
};

class TFenwick {
public:
    explicit TFenwick(const std::vector<uint32_t>& values);

    uint64_t Prefix(size_t end) const;
    void Subtract(size_t index, uint32_t value);
    // Return the vertex containing a zero-based cumulative frequency.
    size_t Find(uint64_t position) const;

private:
    std::vector<uint64_t> Tree;
    size_t TopBit = 1;
};

} // namespace NGraphCompressor
