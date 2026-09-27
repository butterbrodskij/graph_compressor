#include "graph_io.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <unordered_map>
#include <utility>

namespace NGraphCompressor {

namespace {

uint32_t ParseNumber(const TBytes& data, size_t& position) {
    uint64_t value = 0;
    do {
        value = value * 10 + data[position++] - '0';
    } while (position < data.size() && data[position] >= '0' && data[position] <= '9');
    return static_cast<uint32_t>(value);
}

void Append64(TBytes& data, uint64_t value) {
    for (unsigned shift = 0; shift != 64; shift += 8) {
        data.push_back(static_cast<uint8_t>(value >> shift));
    }
}

uint64_t Read64(const TBytes& data, size_t& position) {
    uint64_t value = 0;
    for (unsigned shift = 0; shift != 64; shift += 8) {
        value |= static_cast<uint64_t>(data[position++]) << shift;
    }
    return value;
}

} // namespace

TBytes ReadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    input.exceptions(std::ios::failbit | std::ios::badbit);
    const std::streamoff size = input.tellg();
    TBytes data(static_cast<size_t>(size));
    input.seekg(0);
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()), size);
    }
    return data;
}

void WriteFile(const std::string& path, const TBytes& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    output.close();
}

TGraph ReadGraph(const std::string& path) {
    TBytes data = ReadFile(path);
    std::vector<TEdge> edges;
    edges.reserve(data.size() / 24);
    size_t position = 0;
    while (position < data.size()) {
        uint32_t source = ParseNumber(data, position);
        ++position; // Tab after the source.
        uint32_t target = ParseNumber(data, position);
        ++position; // Tab after the target.
        const uint32_t weight = ParseNumber(data, position);
        if (position < data.size() && data[position] == '\r') {
            ++position;
        }
        if (position < data.size()) {
            ++position; // Line feed, unless the last line ends at EOF.
        }
        if (source > target) {
            std::swap(source, target);
        }
        edges.push_back({source, target, static_cast<uint8_t>(weight)});
    }
    TBytes().swap(data);

    const auto less = [](const TEdge& left, const TEdge& right) {
        return left.Source < right.Source ||
               (left.Source == right.Source && left.Target < right.Target);
    };
    if (!std::is_sorted(edges.begin(), edges.end(), less)) {
        std::sort(edges.begin(), edges.end(), less);
    }
    TGraph graph;
    graph.Ids.reserve(edges.size() * 2);
    for (const TEdge& edge : edges) {
        graph.Ids.push_back(edge.Source);
        graph.Ids.push_back(edge.Target);
    }
    std::sort(graph.Ids.begin(), graph.Ids.end());
    graph.Ids.erase(std::unique(graph.Ids.begin(), graph.Ids.end()), graph.Ids.end());
    std::unordered_map<uint32_t, uint32_t> indices;
    indices.reserve(graph.Ids.size());
    for (size_t i = 0; i < graph.Ids.size(); ++i) {
        indices.emplace(graph.Ids[i], static_cast<uint32_t>(i));
    }
    size_t nonLoops = 0;
    for (TEdge edge : edges) {
        edge.Source = indices.at(edge.Source);
        edge.Target = indices.at(edge.Target);
        if (edge.Source == edge.Target) {
            graph.Loops.push_back(edge);
        } else {
            edges[nonLoops++] = edge;
        }
    }
    edges.resize(nonLoops);
    graph.Edges = std::move(edges);
    return graph;
}

void WriteGraph(const std::string& path, const TGraph& graph) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    std::array<char, 65536> buffer;
    size_t used = 0;
    const auto flush = [&]() {
        output.write(buffer.data(), static_cast<std::streamsize>(used));
        used = 0;
    };
    const auto emit = [&](const TEdge& edge) {
        if (buffer.size() - used < 32) {
            flush();
        }
        char* cursor = buffer.data() + used;
        char* const end = buffer.data() + buffer.size();
        cursor = std::to_chars(cursor, end, graph.Ids[edge.Source]).ptr;
        *cursor++ = '\t';
        cursor = std::to_chars(cursor, end, graph.Ids[edge.Target]).ptr;
        *cursor++ = '\t';
        cursor = std::to_chars(cursor, end, static_cast<unsigned>(edge.Weight)).ptr;
        *cursor++ = '\n';
        used = static_cast<size_t>(cursor - buffer.data());
    };
    // Merge loops back into their source rows to produce canonical TSV order.
    size_t loop = 0;
    for (const TEdge& edge : graph.Edges) {
        while (loop < graph.Loops.size() && graph.Loops[loop].Source <= edge.Source) {
            emit(graph.Loops[loop++]);
        }
        emit(edge);
    }
    while (loop < graph.Loops.size()) {
        emit(graph.Loops[loop++]);
    }
    flush();
    output.close();
}

TBytes Pack(const TArchive& archive) {
    TBytes data = {archive.IdParameter, archive.DegreeParameter};
    Append64(data, archive.VertexCount);
    Append64(data, archive.LoopCount);
    for (const TBytes* section :
         {&archive.Ids, &archive.Degrees, &archive.Adjacency, &archive.Loops}) {
        Append64(data, section->size());
        data.insert(data.end(), section->begin(), section->end());
    }
    data.insert(data.end(), archive.Weights.begin(), archive.Weights.end());
    return data;
}

TArchive Unpack(const TBytes& data) {
    TArchive archive;
    archive.IdParameter = data[0];
    archive.DegreeParameter = data[1];
    size_t position = 2;
    archive.VertexCount = Read64(data, position);
    archive.LoopCount = Read64(data, position);
    for (TBytes* section : {&archive.Ids, &archive.Degrees, &archive.Adjacency, &archive.Loops}) {
        const size_t length = static_cast<size_t>(Read64(data, position));
        section->assign(data.begin() + position, data.begin() + position + length);
        position += length;
    }
    archive.Weights.assign(data.begin() + position, data.end());
    return archive;
}

} // namespace NGraphCompressor
