#pragma once

#include <mutex>
#include <string>
#include <vector>

struct llama_context;
struct llama_model;
struct llama_vocab;

namespace kk::zenz
{

enum class ScoreMode
{
    WholeCandidate,
    DiffTokenSuffix,
};

struct ScoreResult
{
    bool ok = false;
    float raw_logprob = 0.0f;
    float avg_logprob = 0.0f;
    int scored_token_count = 0;
    int skipped_prefix_token_count = 0;
};

class Runner
{
public:
    struct Options
    {
        std::string model_path;
        int n_ctx = 512;
        int n_batch = 512;
        int threads = -1;
        int gpu_layers = 0;
        bool use_mmap = true;
        bool offload_kqv = false;

        std::string left;
        std::string profile;
        std::string topic;
        std::string style;
        std::string preference;

        bool verbose = false;
        bool silence_logs = true;
    };

    explicit Runner(const Options &opt);
    ~Runner();

    Runner(const Runner &) = delete;
    Runner &operator=(const Runner &) = delete;

    std::vector<ScoreResult> score_candidates(
        const std::string &input_hira_utf8,
        const std::vector<std::string> &candidates_utf8,
        ScoreMode score_mode);

private:
    struct Impl;

    Options opt_;
    llama_model *model_ = nullptr;
    llama_context *ctx_ = nullptr;
    const llama_vocab *vocab_ = nullptr;
    Impl *impl_ = nullptr;
    std::mutex mu_;
};

} // namespace kk::zenz
