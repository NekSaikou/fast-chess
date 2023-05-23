#pragma once

#include <map>
#include <mutex>
#include <string>

#include <matchmaking/types/stats.hpp>

namespace fast_chess {

using stats_map = std::map<std::string, std::map<std::string, Stats>>;

class Result {
   public:
    void updateStats(std::string_view engine1, std::string_view engine2, const Stats& stats) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_[std::string(engine1)][std::string(engine2)] += stats;
    }

    /// @brief Stats of engine1 vs engine2, adjusted with the perspective
    /// @param engine1
    /// @param engine2
    /// @return
    [[nodiscard]] Stats getStats(std::string_view engine1, std::string_view engine2) {
        std::lock_guard<std::mutex> lock(results_mutex_);

        auto stats1 = results_[std::string(engine1)][std::string(engine2)];
        auto stats2 = results_[std::string(engine2)][std::string(engine1)];

        // we need to collect the results of engine1 vs engine2 and engine2 vs engine1
        // and combine them so that engine2's wins are engine1's losses and vice versa
        return stats1 + ~stats2;
    }

    [[nodiscard]] stats_map getResults() {
        std::lock_guard<std::mutex> lock(results_mutex_);
        return results_;
    }

    void setResults(const stats_map& results) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_ = results;
    }

   private:
    /// @brief tracks the engine results
    stats_map results_;
    std::mutex results_mutex_;
};

}  // namespace fast_chess