#include "codec.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace NGraphCompressor;
    std::string mode;
    std::string input;
    std::string output;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-s" || argument == "-d") {
            mode = argument;
        } else if ((argument == "-i" || argument == "-o") && i + 1 < argc) {
            (argument == "-i" ? input : output) = argv[++i];
        } else {
            return 1;
        }
    }
    if (mode.empty() || input.empty() || output.empty()) {
        return 1;
    }

    try {
        if (mode == "-s") {
            WriteFile(output, Pack(Compress(ReadGraph(input))));
        } else {
            WriteGraph(output, Decompress(Unpack(ReadFile(input))));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
