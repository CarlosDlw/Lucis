#pragma once

#include <string>
#include <string_view>
#include <random>
#include <cstdint>

class Mutator {
public:
    Mutator();

    /// Apply one random mutation to the source, return the mutated result.
    std::string mutate(std::string_view source);

    /// Number of supported mutation strategies.
    size_t strategyCount() const { return strategies_.size(); }

    /// Name of a strategy by index.
    std::string_view strategyName(size_t idx) const;

    using StrategyFn = std::string (*)(std::string_view, std::mt19937&);

private:
    std::mt19937 rng_;

    static std::vector<StrategyFn> strategies_;
    static std::vector<std::string_view> strategyNames_;
};
