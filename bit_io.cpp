#include "bit_io.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace NGraphCompressor {

void TBitWriter::Write(uint64_t value, unsigned count) {
    while (count != 0) {
        if (Used == 0) {
            Data.push_back(0);
        }
        const unsigned take = std::min(count, 8 - Used);
        count -= take;
        Data.back() |=
            static_cast<uint8_t>(((value >> count) & ((1u << take) - 1)) << (8 - Used - take));
        Used = (Used + take) % 8;
    }
}

void TBitWriter::Zeros(uint64_t count) {
    if (Used != 0 && count != 0) {
        const unsigned take = static_cast<unsigned>(std::min<uint64_t>(count, 8 - Used));
        Write(0, take);
        count -= take;
    }
    Data.insert(Data.end(), static_cast<size_t>(count / 8), 0);
    Write(0, static_cast<unsigned>(count % 8));
}

void TBitWriter::Rice(uint64_t value, unsigned parameter) {
    Zeros(value >> parameter);
    Write(1, 1);
    Write(value, parameter);
}

void TBitWriter::Uniform(uint64_t value, uint64_t range) {
    unsigned bits = 0;
    for (uint64_t size = range; size > 1; size >>= 1) {
        ++bits;
    }
    const uint64_t cutoff = (uint64_t{1} << (bits + 1)) - range;
    if (value < cutoff) {
        Write(value, bits);
    } else {
        Write(value + cutoff, bits + 1);
    }
}

TBytes TBitWriter::Take() {
    return std::move(Data);
}

TBitReader::TBitReader(const TBytes& data)
    : Data(data)
{}

uint64_t TBitReader::Read(unsigned count) {
    uint64_t value = 0;
    while (count != 0) {
        const unsigned used = static_cast<unsigned>(Position % 8);
        const unsigned take = std::min(count, 8 - used);
        value =
            (value << take) | ((Data[Position / 8] >> (8 - used - take)) & ((1u << take) - 1));
        Position += take;
        count -= take;
    }
    return value;
}

uint64_t TBitReader::Rice(unsigned parameter) {
    uint64_t quotient = 0;
    while (Read(1) == 0) {
        ++quotient;
    }
    return (quotient << parameter) | Read(parameter);
}

uint64_t TBitReader::Uniform(uint64_t range) {
    unsigned bits = 0;
    for (uint64_t size = range; size > 1; size >>= 1) {
        ++bits;
    }
    const uint64_t cutoff = (uint64_t{1} << (bits + 1)) - range;
    const uint64_t value = Read(bits);
    return value < cutoff ? value : ((value << 1) | Read(1)) - cutoff;
}

unsigned TBitReader::ArithmeticBit() {
    if (Position < static_cast<uint64_t>(Data.size()) * 8) {
        return static_cast<unsigned>(Read(1));
    }
    return 0;
}

unsigned ChooseRice(const std::vector<uint32_t>& values) {
    uint64_t bestCost = std::numeric_limits<uint64_t>::max();
    unsigned best = 0;
    for (unsigned parameter = 0; parameter <= 32; ++parameter) {
        uint64_t cost = static_cast<uint64_t>(values.size()) * (parameter + 1);
        for (uint32_t value : values) {
            cost += static_cast<uint64_t>(value) >> parameter;
        }
        cost = (cost + 7) / 8;
        if (cost < bestCost) {
            bestCost = cost;
            best = parameter;
        }
    }
    return best;
}

TBytes EncodeRice(const std::vector<uint32_t>& values, unsigned parameter) {
    TBitWriter writer;
    for (uint32_t value : values) {
        writer.Rice(value, parameter);
    }
    return writer.Take();
}

} // namespace NGraphCompressor
