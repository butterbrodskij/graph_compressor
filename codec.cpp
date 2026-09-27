#include "codec.h"

#include "arithmetic.h"
#include "bit_io.h"

namespace NGraphCompressor {

namespace {

// Interpolation is used only for the sorted list of loop vertices.
template <class TGetValue>
void EncodeInterpolative(TBitWriter& writer, size_t begin, size_t end, uint64_t lower,
                         uint64_t upper, const TGetValue& getValue) {
    if (begin == end) {
        return;
    }
    const size_t middle = begin + (end - begin) / 2;
    const uint64_t minimum = lower + middle - begin;
    const uint64_t maximum = upper - (end - middle - 1);
    const uint64_t value = getValue(middle);
    writer.Uniform(value - minimum, maximum - minimum + 1);
    if (middle != begin) {
        EncodeInterpolative(writer, begin, middle, lower, value - 1, getValue);
    }
    if (middle + 1 != end) {
        EncodeInterpolative(writer, middle + 1, end, value + 1, upper, getValue);
    }
}

void DecodeInterpolative(TBitReader& reader, std::vector<uint32_t>& values, size_t begin,
                         size_t end, uint64_t lower, uint64_t upper) {
    if (begin == end) {
        return;
    }
    const size_t middle = begin + (end - begin) / 2;
    const uint64_t minimum = lower + middle - begin;
    const uint64_t maximum = upper - (end - middle - 1);
    const uint64_t value = minimum + reader.Uniform(maximum - minimum + 1);
    values[middle] = static_cast<uint32_t>(value);
    if (middle != begin) {
        DecodeInterpolative(reader, values, begin, middle, lower, value - 1);
    }
    if (middle + 1 != end) {
        DecodeInterpolative(reader, values, middle + 1, end, value + 1, upper);
    }
}

// Midpoint coding in a degree-weighted interval. Bounds reserve one distinct
// vertex per remaining list element. Frequencies stay fixed within each row;
// only after the row is complete are its incident degrees subtracted.
void EncodeWeightedList(TArithmeticEncoder& encoder, const TFenwick& tree,
                        const std::vector<uint32_t>& remaining,
                        const std::vector<TEdge>& edges, size_t begin, size_t end,
                        size_t lower, size_t upper) {
    if (begin == end) {
        return;
    }
    const size_t middle = begin + (end - begin) / 2;
    const size_t minimum = lower + middle - begin;
    const size_t maximum = upper - (end - middle - 1);
    const size_t value = edges[middle].Target;
    const uint64_t base = tree.Prefix(minimum);
    const uint64_t total = tree.Prefix(maximum + 1) - base;
    encoder.Encode(tree.Prefix(value) - base, remaining[value], total);
    if (middle != begin) {
        EncodeWeightedList(encoder, tree, remaining, edges, begin, middle, lower, value - 1);
    }
    if (middle + 1 != end) {
        EncodeWeightedList(encoder, tree, remaining, edges, middle + 1, end, value + 1, upper);
    }
}

void DecodeWeightedList(TArithmeticDecoder& decoder, const TFenwick& tree,
                        const std::vector<uint32_t>& remaining,
                        std::vector<uint32_t>& values, size_t begin, size_t end,
                        size_t lower, size_t upper) {
    if (begin == end) {
        return;
    }
    const size_t middle = begin + (end - begin) / 2;
    const size_t minimum = lower + middle - begin;
    const size_t maximum = upper - (end - middle - 1);
    const uint64_t base = tree.Prefix(minimum);
    const uint64_t total = tree.Prefix(maximum + 1) - base;
    const size_t value = tree.Find(base + decoder.Position(total));
    decoder.Advance(tree.Prefix(value) - base, remaining[value], total);
    values[middle] = static_cast<uint32_t>(value);
    if (middle != begin) {
        DecodeWeightedList(decoder, tree, remaining, values, begin, middle, lower, value - 1);
    }
    if (middle + 1 != end) {
        DecodeWeightedList(decoder, tree, remaining, values, middle + 1, end, value + 1, upper);
    }
}

void EncodeDegree(const TGraph& graph, TArchive& archive) {
    std::vector<uint32_t> remaining(graph.Ids.size(), 0);
    for (const TEdge& edge : graph.Edges) {
        ++remaining[edge.Source];
        ++remaining[edge.Target];
    }
    archive.DegreeParameter = static_cast<uint8_t>(ChooseRice(remaining));
    archive.Degrees = EncodeRice(remaining, archive.DegreeParameter);
    TFenwick tree(remaining);
    TArithmeticEncoder encoder;
    size_t begin = 0;
    for (size_t source = 0; source < remaining.size(); ++source) {
        const size_t count = remaining[source];
        tree.Subtract(source, remaining[source]);
        remaining[source] = 0;
        const size_t end = begin + count;
        if (count != 0) {
            EncodeWeightedList(encoder, tree, remaining, graph.Edges, begin, end, source + 1,
                               remaining.size() - 1);
        }
        for (size_t i = begin; i < end; ++i) {
            const uint32_t target = graph.Edges[i].Target;
            --remaining[target];
            tree.Subtract(target, 1);
        }
        begin = end;
    }
    archive.Adjacency = encoder.Finish();
}

void DecodeDegree(const TArchive& archive, TGraph& graph) {
    std::vector<uint32_t> remaining(static_cast<size_t>(archive.VertexCount));
    TBitReader degreeReader(archive.Degrees);
    for (uint32_t& degree : remaining) {
        degree = static_cast<uint32_t>(degreeReader.Rice(archive.DegreeParameter));
    }
    TFenwick tree(remaining);
    TArithmeticDecoder decoder(archive.Adjacency);
    std::vector<uint32_t> neighbors;
    size_t weight = static_cast<size_t>(archive.LoopCount);
    for (size_t source = 0; source < remaining.size(); ++source) {
        const size_t count = remaining[source];
        tree.Subtract(source, remaining[source]);
        remaining[source] = 0;
        neighbors.resize(count);
        if (count != 0) {
            DecodeWeightedList(decoder, tree, remaining, neighbors, 0, count, source + 1,
                               remaining.size() - 1);
        }
        for (uint32_t target : neighbors) {
            --remaining[target];
            tree.Subtract(target, 1);
            graph.Edges.push_back(
                {static_cast<uint32_t>(source), target, archive.Weights[weight++]});
        }
    }
}

} // namespace

TArchive Compress(const TGraph& graph) {
    TArchive archive;
    archive.VertexCount = graph.Ids.size();
    archive.LoopCount = graph.Loops.size();
    std::vector<uint32_t> gaps;
    gaps.reserve(graph.Ids.size());
    uint64_t previous = 0;
    for (uint32_t id : graph.Ids) {
        gaps.push_back(static_cast<uint32_t>(id - previous));
        previous = static_cast<uint64_t>(id) + 1;
    }
    archive.IdParameter = static_cast<uint8_t>(ChooseRice(gaps));
    archive.Ids = EncodeRice(gaps, archive.IdParameter);
    TBitWriter loopWriter;
    if (!graph.Loops.empty()) {
        EncodeInterpolative(loopWriter, 0, graph.Loops.size(), 0, graph.Ids.size() - 1,
                            [&](size_t index) { return graph.Loops[index].Source; });
    }
    archive.Loops = loopWriter.Take();
    archive.Weights.reserve(graph.Edges.size() + graph.Loops.size());
    for (const TEdge& loop : graph.Loops) {
        archive.Weights.push_back(loop.Weight);
    }
    for (const TEdge& edge : graph.Edges) {
        archive.Weights.push_back(edge.Weight);
    }

    EncodeDegree(graph, archive);
    return archive;
}

TGraph Decompress(const TArchive& archive) {
    TGraph graph;
    graph.Ids.reserve(static_cast<size_t>(archive.VertexCount));
    TBitReader idReader(archive.Ids);
    uint64_t previous = 0;
    for (uint64_t i = 0; i < archive.VertexCount; ++i) {
        const uint64_t id = previous + idReader.Rice(archive.IdParameter);
        graph.Ids.push_back(static_cast<uint32_t>(id));
        previous = id + 1;
    }
    std::vector<uint32_t> loops(static_cast<size_t>(archive.LoopCount));
    TBitReader loopReader(archive.Loops);
    if (!loops.empty()) {
        DecodeInterpolative(loopReader, loops, 0, loops.size(), 0, graph.Ids.size() - 1);
    }
    for (size_t i = 0; i < loops.size(); ++i) {
        graph.Loops.push_back({loops[i], loops[i], archive.Weights[i]});
    }
    graph.Edges.reserve(archive.Weights.size() - static_cast<size_t>(archive.LoopCount));
    DecodeDegree(archive, graph);
    return graph;
}

} // namespace NGraphCompressor
