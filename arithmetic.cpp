#include "arithmetic.h"

namespace NGraphCompressor {

namespace {

constexpr uint64_t QUARTER = uint64_t{1} << 30;
constexpr uint64_t HALF = uint64_t{1} << 31;

} // namespace

void TArithmeticEncoder::Encode(uint64_t cumulative, uint64_t frequency, uint64_t total) {
    const uint64_t range = High - Low + 1;
    High = Low + range * (cumulative + frequency) / total - 1;
    Low += range * cumulative / total;
    while (true) {
        if (High < HALF) {
            Emit(0);
        } else if (Low >= HALF) {
            Emit(1);
            Low -= HALF;
            High -= HALF;
        } else if (Low >= QUARTER && High < 3 * QUARTER) {
            ++Pending;
            Low -= QUARTER;
            High -= QUARTER;
        } else {
            break;
        }
        Low *= 2;
        High = High * 2 + 1;
    }
}

TBytes TArithmeticEncoder::Finish() {
    ++Pending;
    Emit(Low >= QUARTER);
    return Writer.Take();
}

void TArithmeticEncoder::Emit(unsigned bit) {
    Writer.Write(bit, 1);
    while (Pending != 0) {
        Writer.Write(1 - bit, 1);
        --Pending;
    }
}

TArithmeticDecoder::TArithmeticDecoder(const TBytes& data)
    : Reader(data)
{
    for (unsigned bit = 0; bit < 32; ++bit) {
        Code = Code * 2 + Reader.ArithmeticBit();
    }
}

uint64_t TArithmeticDecoder::Position(uint64_t total) const {
    return ((Code - Low + 1) * total - 1) / (High - Low + 1);
}

void TArithmeticDecoder::Advance(uint64_t cumulative, uint64_t frequency, uint64_t total) {
    const uint64_t range = High - Low + 1;
    High = Low + range * (cumulative + frequency) / total - 1;
    Low += range * cumulative / total;
    while (true) {
        if (High < HALF) {
            // No translation is needed for the lower half.
        } else if (Low >= HALF) {
            Low -= HALF;
            High -= HALF;
            Code -= HALF;
        } else if (Low >= QUARTER && High < 3 * QUARTER) {
            Low -= QUARTER;
            High -= QUARTER;
            Code -= QUARTER;
        } else {
            break;
        }
        Low *= 2;
        High = High * 2 + 1;
        Code = Code * 2 + Reader.ArithmeticBit();
    }
}

TFenwick::TFenwick(const std::vector<uint32_t>& values)
    : Tree(values.size() + 1, 0)
{
    for (size_t i = 1; i < Tree.size(); ++i) {
        Tree[i] += values[i - 1];
        const size_t parent = i + (i & (~i + 1));
        if (parent < Tree.size()) {
            Tree[parent] += Tree[i];
        }
    }
    while (TopBit <= values.size() / 2) {
        TopBit *= 2;
    }
}

uint64_t TFenwick::Prefix(size_t end) const {
    uint64_t sum = 0;
    for (; end != 0; end &= end - 1) {
        sum += Tree[end];
    }
    return sum;
}

void TFenwick::Subtract(size_t index, uint32_t value) {
    for (++index; index < Tree.size(); index += index & (~index + 1)) {
        Tree[index] -= value;
    }
}

size_t TFenwick::Find(uint64_t position) const {
    size_t index = 0;
    for (size_t step = TopBit; step != 0; step >>= 1) {
        const size_t next = index + step;
        if (next < Tree.size() && Tree[next] <= position) {
            index = next;
            position -= Tree[next];
        }
    }
    return index;
}

} // namespace NGraphCompressor
