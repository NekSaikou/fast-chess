#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

#include <chess.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <pgn/pgn_builder.hpp>
#include <util/logger/logger.hpp>
#include <util/rand.hpp>
#include <util/scope_guard.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const options::Tournament& tournament_config,
                       const std::vector<EngineConfiguration>& engine_configs)
    : BaseTournament(tournament_config, engine_configs) {
    // Initialize the SPRT test
    sprt_ = SPRT(tournament_options_.sprt.alpha, tournament_options_.sprt.beta,
                 tournament_options_.sprt.elo0, tournament_options_.sprt.elo1,
                 tournament_options_.sprt.logistic_bounds);
}

void RoundRobin::start() {
    BaseTournament::start();

    // Wait for games to finish
    while (match_count_ < total_ && !atomic::stop) {
    }
}

void RoundRobin::create() {
    total_ = (engine_configs_.size() * (engine_configs_.size() - 1) / 2) *
             tournament_options_.rounds * tournament_options_.games;

    const auto create_match = [this](std::size_t i, std::size_t j, std::size_t round_id, int g,
                                     pgn::Opening opening) {
        assert(g < 2);

        constexpr static auto normalize_stm_configs = [](const pair_config& configs,
                                                         const chess::Color stm) {
            // swap players if the opening is for black, to ensure that
            // reporting the result is always white vs black
            if (stm == chess::Color::BLACK) {
                return std::pair{configs.second, configs.first};
            }

            return configs;
        };

        constexpr static auto normalize_stats = [](const Stats& stats, const chess::Color stm) {
            // swap stats if the opening is for black, to ensure that
            // reporting the result is always white vs black
            if (stm == chess::Color::BLACK) {
                return ~stats;
            }

            return stats;
        };

        const std::size_t game_id = round_id * tournament_options_.games + (g + 1);
        const auto stm            = opening.stm;
        const auto first          = engine_configs_[i];
        const auto second         = engine_configs_[j];
        auto configs              = std::pair{engine_configs_[i], engine_configs_[j]};

        if (g == 1) {
            std::swap(configs.first, configs.second);
        }

        // callback functions, do not capture by reference
        const auto start = [this, configs, game_id, stm]() {
            output_->startGame(normalize_stm_configs(configs, stm), game_id, total_);
        };

        // callback functions, do not capture by reference
        const auto finish = [this, configs, first, second, game_id, round_id, stm](
                                const Stats& stats, const std::string& reason) {
            const auto normalized_configs = normalize_stm_configs(configs, stm);
            const auto normalized_stats   = normalize_stats(stats, stm);

            output_->endGame(normalized_configs, normalized_stats, reason, game_id);

            bool report = true;

            if (tournament_options_.report_penta)
                report = result_.updatePairStats(configs, first.name, stats, round_id);
            else
                result_.updateStats(configs, stats);

            // game_id starts 1 and round_id starts 0
            const auto interval_index = tournament_options_.report_penta ? round_id + 1 : game_id;

            // Only print the interval if the pair is complete or we are not tracking
            // penta stats.
            if (report && interval_index % tournament_options_.ratinginterval == 0) {
                const auto updated_stats = result_.getStats(first.name, second.name);

                output_->printInterval(sprt_, updated_stats, first.name, second.name);
            }

            updateSprtStatus({first, second});

            match_count_++;
        };

        playGame(configs, start, finish, opening, round_id);
    };

    for (std::size_t i = 0; i < engine_configs_.size(); i++) {
        for (std::size_t j = i + 1; j < engine_configs_.size(); j++) {
            int offset = initial_matchcount_ / tournament_options_.games;
            for (int k = offset; k < tournament_options_.rounds; k++) {
                // both players get the same opening
                const auto opening = book_.fetch();

                for (int g = 0; g < tournament_options_.games; g++) {
                    pool_.enqueue(create_match, i, j, k, g, opening);
                }
            }
        }
    }
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs) {
    if (!sprt_.isValid()) return;

    const auto stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const auto llr   = sprt_.getLLR(stats, tournament_options_.report_penta);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::log<Logger::Level::INFO>("SPRT test finished: " + sprt_.getBounds() + " " +
                                         sprt_.getElo());

        output_->printInterval(sprt_, stats, engine_configs[0].name, engine_configs[1].name);
        output_->endTournament();

        stop();
    }
}

}  // namespace fast_chess
