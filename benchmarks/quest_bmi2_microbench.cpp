#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "quest/src/core/bitwise.hpp"

volatile qindex sinkValue = 0;

template <size_t N>
qindex makeMask(const std::array<int, N>& indices, qindex pattern) {
    qindex mask = 0;
    for (size_t i = 0; i < N; ++i)
        if ((pattern >> i) & 1)
            mask |= QINDEX_ONE << indices[i];
    return mask;
}

template <size_t N>
double benchGet(const std::string& name, const std::array<int, N>& indices, const std::vector<qindex>& inputs, qindex ampMask, qindex iterations, int repeats) {
    double best = std::numeric_limits<double>::max();
    size_t inputMask = inputs.size() - 1;

    for (int r = 0; r < repeats; ++r) {
        qindex acc = static_cast<qindex>(0x13579BDF);
        auto start = std::chrono::steady_clock::now();

        for (qindex i = 0; i < iterations; ++i) {
            qindex n = (inputs[static_cast<size_t>(i) & inputMask] + acc) & ampMask;
            acc ^= getValueOfBits(n, indices.data(), static_cast<int>(N)) + (i & 7);
        }

        auto stop = std::chrono::steady_clock::now();
        sinkValue ^= acc;
        double nsPerCall = std::chrono::duration<double, std::nano>(stop - start).count() / static_cast<double>(iterations);
        if (nsPerCall < best)
            best = nsPerCall;
    }

    std::cout << std::left << std::setw(30) << name << " " << std::fixed << std::setprecision(3) << best << " ns/call\n";
    return best;
}

template <size_t N>
double benchInsert(const std::string& name, const std::array<int, N>& indices, const std::vector<qindex>& inputs, qindex valueMask, qindex insertedMask, qindex iterations, int repeats) {
    double best = std::numeric_limits<double>::max();
    size_t inputMask = inputs.size() - 1;

    for (int r = 0; r < repeats; ++r) {
        qindex acc = static_cast<qindex>(0x2468ACE0);
        auto start = std::chrono::steady_clock::now();

        for (qindex i = 0; i < iterations; ++i) {
            qindex n = (inputs[static_cast<size_t>(i) & inputMask] + acc) & valueMask;
            acc ^= insertBitsWithMaskedValues(n, indices.data(), static_cast<int>(N), insertedMask) + (i & 15);
        }

        auto stop = std::chrono::steady_clock::now();
        sinkValue ^= acc;
        double nsPerCall = std::chrono::duration<double, std::nano>(stop - start).count() / static_cast<double>(iterations);
        if (nsPerCall < best)
            best = nsPerCall;
    }

    std::cout << std::left << std::setw(30) << name << " " << std::fixed << std::setprecision(3) << best << " ns/call\n";
    return best;
}

int main(int argc, char** argv) {
    qindex iterations = (argc > 1) ? static_cast<qindex>(std::stoll(argv[1])) : static_cast<qindex>(50000000);
    int repeats = (argc > 2) ? std::stoi(argv[2]) : 9;
    qindex nineQubitMask = (QINDEX_ONE << 9) - QINDEX_ONE;

#if defined(QUEST_USE_BMI2_INTRINSICS)
    std::cout << "variant: bmi2\n";
#else
    std::cout << "variant: fallback\n";
#endif
    std::cout << "iterations: " << iterations << ", repeats: " << repeats << "\n";

    std::vector<qindex> inputs(1 << 15);
    qindex state = static_cast<qindex>(0x123456789ABCDEFULL);
    for (qindex& input : inputs) {
        state = state * static_cast<qindex>(0x5851F42D4C957F2DULL) + static_cast<qindex>(0x14057B7EF767814FULL);
        input = state;
    }

    const std::array<int, 2> inds2 = {2, 7};
    const std::array<int, 5> inds5 = {0, 2, 4, 6, 8};
    const std::array<int, 6> inds6 = {0, 1, 3, 5, 7, 8};

    benchGet("getValueOfBits 2 bits", inds2, inputs, nineQubitMask, iterations, repeats);
    benchGet("getValueOfBits 5 bits", inds5, inputs, nineQubitMask, iterations, repeats);
    benchGet("getValueOfBits 6 bits", inds6, inputs, nineQubitMask, iterations, repeats);

    benchInsert("insertBitsWithMask 2 bits", inds2, inputs, (QINDEX_ONE << 7) - QINDEX_ONE, makeMask(inds2, 0b01), iterations, repeats);
    benchInsert("insertBitsWithMask 5 bits", inds5, inputs, (QINDEX_ONE << 4) - QINDEX_ONE, makeMask(inds5, 0b10101), iterations, repeats);
    benchInsert("insertBitsWithMask 6 bits", inds6, inputs, (QINDEX_ONE << 3) - QINDEX_ONE, makeMask(inds6, 0b101011), iterations, repeats);

    std::cerr << "sink: " << sinkValue << "\n";
    return 0;
}
