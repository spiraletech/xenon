#pragma once

#include "xenon/candidate_swarm_engine.hpp"
#include "xenon/session_os.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace xenon {

enum class ProducerStage { Planned, Generated, Auditioned, Revised, Mixed, Mastered, AwaitingApproval, Complete, Failed };

struct AutonomousProducerGoal {
    std::string project_id{"autonomous-project"};
    std::string goal;
    std::size_t track_count{1};
    double duration_seconds{30.0};
    double bpm{0.0};
    std::string key;
    std::uint64_t seed{1};
    std::uint32_t max_revision_passes{1};
    double minimum_candidate_score{0.45};
    bool require_user_approval{true};
};

struct ProducerCheckpoint {
    ProducerStage stage{ProducerStage::Planned};
    std::size_t track_index{0};
    std::string graph_node_id;
    std::string note;
};

struct AutonomousTrackPlan {
    std::size_t track_index{0};
    std::string title;
    GenerationRequest request;
};

struct AutonomousTrackResult {
    AutonomousTrackPlan plan;
    CandidateRecord winner;
    std::vector<CortexCritique> critiques;
    std::uint32_t revision_passes{0};
    std::string generation_node_id;
    std::string revision_node_id;
    std::string mix_node_id;
    std::string master_node_id;
    bool approved{false};
};

struct AutonomousProducerResult {
    std::string project_id;
    ProducerStage stage{ProducerStage::Planned};
    std::vector<AutonomousTrackResult> tracks;
    std::vector<ProducerCheckpoint> checkpoints;
    std::string session_snapshot;
    bool awaiting_user_approval{false};
};

class AutonomousProducer {
public:
    explicit AutonomousProducer(GenerationPipeline pipeline);

    [[nodiscard]] std::vector<AutonomousTrackPlan> plan(const AutonomousProducerGoal& goal) const;
    [[nodiscard]] AutonomousProducerResult produce(const AutonomousProducerGoal& goal, const std::filesystem::path& output_directory);
    void approve(AutonomousProducerResult& result, std::size_t track_index, bool approved) const;
    void finalize(AutonomousProducerResult& result) const;
    void validate(const AutonomousProducerGoal& goal) const;

private:
    [[nodiscard]] GenerationRequest revision_request(const AutonomousTrackPlan& track, const CandidateRecord& winner, const CortexCritique& critique, std::uint32_t pass) const;
    GenerationPipeline pipeline_;
};

} // namespace xenon
