#include "zenz/reranker.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace kk::zenz
{
namespace
{

static std::vector<float> min_max_normalize(const std::vector<float> &values)
{
    std::vector<float> out(values.size(), 1.0f);
    if (values.empty())
    {
        return out;
    }

    auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    const float min_v = *min_it;
    const float max_v = *max_it;
    const float span = max_v - min_v;

    if (span <= 1e-6f)
    {
        return out;
    }

    for (size_t i = 0; i < values.size(); ++i)
    {
        out[i] = (values[i] - min_v) / span;
    }
    return out;
}

} // namespace

std::vector<RankedCandidate> rerank_candidates(
    Runner &runner,
    const std::vector<Candidate> &candidates,
    const Config &config)
{
    std::vector<RankedCandidate> ranked;
    ranked.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        RankedCandidate rc;
        static_cast<Candidate &>(rc) = candidates[i];
        rc.original_index = i;
        ranked.push_back(std::move(rc));
    }

    std::unordered_map<std::string, std::vector<size_t>> groups;
    groups.reserve(ranked.size());
    for (size_t i = 0; i < ranked.size(); ++i)
    {
        groups[ranked[i].yomi_utf8].push_back(i);
    }

    for (const auto &entry : groups)
    {
        const std::string &yomi = entry.first;
        const std::vector<size_t> &idxs = entry.second;

        std::vector<std::string> texts;
        texts.reserve(idxs.size());
        for (size_t idx : idxs)
        {
            texts.push_back(ranked[idx].text_utf8);
        }

        const auto results = runner.score_candidates(yomi, texts, config.score_mode);
        if (results.size() != idxs.size())
        {
            continue;
        }

        for (size_t j = 0; j < idxs.size(); ++j)
        {
            RankedCandidate &dst = ranked[idxs[j]];
            const ScoreResult &src = results[j];
            dst.zenz_scored = src.ok;
            dst.zenz_raw_logprob = src.raw_logprob;
            dst.zenz_avg_logprob = src.avg_logprob;
            dst.scored_token_count = src.scored_token_count;
            dst.skipped_prefix_token_count = src.skipped_prefix_token_count;
        }
    }

    std::vector<float> base_values;
    base_values.reserve(ranked.size());
    for (const auto &cand : ranked)
    {
        base_values.push_back(-static_cast<float>(cand.base_cost));
    }
    const auto base_norm = min_max_normalize(base_values);

    std::vector<float> zenz_values;
    zenz_values.reserve(ranked.size());
    for (const auto &cand : ranked)
    {
        if (!cand.zenz_scored)
        {
            zenz_values.push_back(-std::numeric_limits<float>::infinity());
            continue;
        }
        zenz_values.push_back(config.use_avg_logprob ? cand.zenz_avg_logprob : cand.zenz_raw_logprob);
    }

    std::vector<float> finite_zenz;
    finite_zenz.reserve(ranked.size());
    for (float v : zenz_values)
    {
        if (std::isfinite(v))
        {
            finite_zenz.push_back(v);
        }
    }
    const auto finite_norm = min_max_normalize(finite_zenz);

    size_t finite_pos = 0;
    for (size_t i = 0; i < ranked.size(); ++i)
    {
        ranked[i].base_score_norm = base_norm[i];
        if (std::isfinite(zenz_values[i]))
        {
            ranked[i].zenz_score_norm = finite_norm[finite_pos++];
        }
        else
        {
            ranked[i].zenz_score_norm = 0.0f;
        }

        if (config.rerank_mode == RerankMode::ZenzOnly)
        {
            ranked[i].fused_score = ranked[i].zenz_scored
                ? ranked[i].zenz_score_norm
                : ranked[i].base_score_norm;
        }
        else
        {
            ranked[i].fused_score =
                config.alpha * ranked[i].base_score_norm +
                config.beta * ranked[i].zenz_score_norm;
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedCandidate &a, const RankedCandidate &b)
                     {
        if (a.fused_score != b.fused_score) return a.fused_score > b.fused_score;
        if (a.zenz_avg_logprob != b.zenz_avg_logprob) return a.zenz_avg_logprob > b.zenz_avg_logprob;
        if (a.base_cost != b.base_cost) return a.base_cost < b.base_cost;
        return a.original_index < b.original_index; });

    return ranked;
}

} // namespace kk::zenz
