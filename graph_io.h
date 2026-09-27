#pragma once

#include "bit_io.h"

#include <string>
#include <vector>

#include <cstdint>

namespace NGraphCompressor {

struct TEdge {
public:
    uint32_t Source;
    uint32_t Target;
    uint8_t Weight;
};

struct TGraph {
public:
    std::vector<uint32_t> Ids;
    std::vector<TEdge> Edges; // Non-loop edges, ordered by (Source, Target).
    std::vector<TEdge> Loops;
};

struct TArchive {
public:
    uint8_t IdParameter = 0;
    uint8_t DegreeParameter = 0;
    uint64_t VertexCount = 0;
    uint64_t LoopCount = 0;
    TBytes Ids;
    TBytes Degrees;
    TBytes Adjacency;
    TBytes Loops;
    TBytes Weights;
};

TBytes ReadFile(const std::string& path);
void WriteFile(const std::string& path, const TBytes& data);
TGraph ReadGraph(const std::string& path);
void WriteGraph(const std::string& path, const TGraph& graph);

// Degree-only format: Rice parameters, vertex/loop counts, four
// length-prefixed bit streams, followed by one weight byte per edge.
TBytes Pack(const TArchive& archive);
TArchive Unpack(const TBytes& data);

} // namespace NGraphCompressor
