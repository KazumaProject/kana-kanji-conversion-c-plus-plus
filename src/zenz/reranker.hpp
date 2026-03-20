#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "zenz/runner.hpp"

namespace kk::zenz
{

enum class RerankMode
{
    ZenzOnly,
    LinearFuse,
};

struct Candidate
{
    std::string text_utf8;
    std::string yomi_utf8;
    int base_cost = 0;
    int type = 0;
    bool has_lr = false;
    int16_t left_id = 0;
    int16_t right_id = 0;
};

struct RankedCandidate : public Candidate
{
    bool zenz_scored = false;
    float zenz_raw_logprob = 0.0f;
    float zenz_avg_logprob = 0.0f;
    float base_score_norm = 0.0f;
    float zenz_score_norm = 0.0f;
    float fused_score = 0.0f;
    int scored_token_count = 0;
    int skipped_prefix_token_count = 0;
    size_t original_index = 0;
};

struct Config
{
    RerankMode rerank_mode = RerankMode::LinearFuse;
    ScoreMode score_mode = ScoreMode::WholeCandidate;
    float alpha = 1.0f;
    float beta = 1.0f;
    bool use_avg_logprob = true;
};

std::vector<RankedCandidate> rerank_candidates(
    Runner &runner,
    const std::vector<Candidate> &candidates,
    const Config &config);

} // namespace kk::zenz
