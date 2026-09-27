#!/usr/bin/env python3

import argparse
import filecmp
import random
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def invoke(binary, *arguments):
    result = subprocess.run(
        [str(binary), *map(str, arguments)], capture_output=True, timeout=300
    )
    assert result.returncode == 0, result.stderr.decode(errors="replace")
    assert not result.stdout, result.stdout
    assert not result.stderr, result.stderr


def tsv(edges, newline="\n"):
    return "".join(f"{a}\t{b}\t{w}{newline}" for a, b, w in edges).encode()


def roundtrip(binary, source, expected, directory):
    archive = directory / "graph.bin"
    output = directory / "output.tsv"
    invoke(binary, "-s", "-i", source, "-o", archive)
    invoke(binary, "-d", "-i", archive, "-o", output)
    filecmp.clear_cache()
    assert filecmp.cmp(expected, output, shallow=False), source.name
    return archive


def cases():
    yield "empty", []
    yield "zero_loop", [(0, 0, 0)]
    yield "max_loop", [(0xFFFFFFFF, 0xFFFFFFFF, 255)]
    yield "extremes", [(0, 0xFFFFFFFF, 255), (0, 0, 0), (0xFFFFFFFF, 0xFFFFFFFF, 1)]
    yield "all_weights", [(i * 1009, i * 1009 + 1, i) for i in range(256)]
    yield "only_loops", [(i * 1709, i * 1709, i % 256) for i in range(200)]
    for center in (0, 50, 100):
        yield f"star_{center}", [(center, i, i % 256) for i in range(101) if i != center]
    yield "path", [(i, i + 1, i % 256) for i in range(300)]
    for n in (2, 3, 16, 65):
        yield f"complete_{n}", [(a, b, (a + b) % 256) for a in range(n) for b in range(a, n)]
    for seed in range(80):
        rng = random.Random(seed)
        n = rng.randrange(1, 130)
        ids = sorted(rng.sample(range(1 << 32), n))
        density = (0.01, 0.05, 0.2, 0.7, 1.0)[seed % 5]
        edges = [(a, b, rng.randrange(256)) for i, a in enumerate(ids) for b in ids[i:]
                 if rng.random() < density]
        yield f"random_{seed}", edges


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, default=ROOT / "run")
    parser.add_argument("--example", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    with tempfile.TemporaryDirectory(prefix="graph-compressor-test-") as temporary:
        directory = Path(temporary)
        expected = directory / "expected.tsv"
        rng = random.Random(48293)
        count = 0
        for index, (name, edges) in enumerate(cases()):
            expected.write_bytes(tsv(sorted((min(a, b), max(a, b), w) for a, b, w in edges)))
            rows = [(b, a, w) if rng.randrange(2) else (a, b, w) for a, b, w in edges]
            rng.shuffle(rows)
            data = tsv(rows, "\r\n" if index % 3 == 0 else "\n")
            if index % 4 == 0:
                data = data.rstrip(b"\r\n")
            source = directory / f"{name}.tsv"
            source.write_bytes(data)
            archive = roundtrip(binary, source, expected, directory)
            compressed = archive.read_bytes()
            invoke(binary, "-s", "-i", expected, "-o", archive)
            assert archive.read_bytes() == compressed, name
            count += 1
        print(f"Roundtrip, format and determinism: {count} graphs", flush=True)
        if args.example is not None:
            source = args.example.resolve()
            roundtrip(binary, source, source, directory)
            print(f"{source.name}: OK", flush=True)


if __name__ == "__main__":
    main()
