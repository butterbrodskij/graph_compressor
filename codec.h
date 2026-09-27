#pragma once

#include "graph_io.h"

namespace NGraphCompressor {

TArchive Compress(const TGraph& graph);
TGraph Decompress(const TArchive& archive);

} // namespace NGraphCompressor
