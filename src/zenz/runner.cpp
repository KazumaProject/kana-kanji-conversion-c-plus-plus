#include "zenz/runner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

#include "llama.h"

namespace kk::zenz
{
namespace
{

struct KvState
{
    std::vector<llama_token> prev_tokens;
};

static void die(const std::string &msg)
{
    throw std::runtime_error(msg);
}

static int default_threads()
{
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    if (hw <= 0)
        hw = 4;
    int t = hw - 2;
    if (t < 1)
        t = 1;
    if (t > 8)
        t = 8;
    return t;
}

static void quiet_llama_log(ggml_log_level, const char *, void *)
{
}

static std::string preprocess_text(std::string s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s)
    {
        if (ch == ' ')
        {
            out.append("\xE3\x80\x80");
        }
        else if (ch == '\n' || ch == '\r')
        {
        }
        else
        {
            out.push_back(ch);
        }
    }
    return out;
}

static std::string hira_to_kata_utf8(const std::string &utf8)
{
    std::string out;
    out.reserve(utf8.size());

    const unsigned char *p = reinterpret_cast<const unsigned char *>(utf8.data());
    const size_t n = utf8.size();
    size_t i = 0;

    auto append_u8 = [&](uint32_t cp)
    {
        if (cp <= 0x7F)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF)
        {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0xFFFF)
        {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    };

    auto decode_one = [&](uint32_t &cp, size_t &adv) -> bool
    {
        adv = 0;
        if (i >= n)
            return false;
        const unsigned char c0 = p[i];
        if (c0 < 0x80)
        {
            cp = c0;
            adv = 1;
            return true;
        }
        if ((c0 & 0xE0) == 0xC0)
        {
            if (i + 1 >= n)
                return false;
            const unsigned char c1 = p[i + 1];
            if ((c1 & 0xC0) != 0x80)
                return false;
            cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            adv = 2;
            return true;
        }
        if ((c0 & 0xF0) == 0xE0)
        {
            if (i + 2 >= n)
                return false;
            const unsigned char c1 = p[i + 1];
            const unsigned char c2 = p[i + 2];
            if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80))
                return false;
            cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            adv = 3;
            return true;
        }
        if ((c0 & 0xF8) == 0xF0)
        {
            if (i + 3 >= n)
                return false;
            const unsigned char c1 = p[i + 1];
            const unsigned char c2 = p[i + 2];
            const unsigned char c3 = p[i + 3];
            if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80))
                return false;
            cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            adv = 4;
            return true;
        }
        return false;
    };

    while (i < n)
    {
        uint32_t cp = 0;
        size_t adv = 0;
        if (!decode_one(cp, adv) || adv == 0)
        {
            out.push_back(static_cast<char>(p[i]));
            ++i;
            continue;
        }
        if (0x3041 <= cp && cp <= 0x3096)
        {
            cp += 0x60;
        }
        append_u8(cp);
        i += adv;
    }

    return out;
}

static std::string build_prompt_v3(
    const std::string &left_context_utf8,
    const std::string &input_hira_utf8,
    const std::string &profile_utf8,
    const std::string &topic_utf8,
    const std::string &style_utf8,
    const std::string &preference_utf8,
    int max_left_len_chars)
{
    const std::string input_tag = "\xEE\xB8\x80";
    const std::string output_tag = "\xEE\xB8\x81";
    const std::string context_tag = "\xEE\xB8\x82";
    const std::string profile_tag = "\xEE\xB8\x83";
    const std::string topic_tag = "\xEE\xB8\x84";
    const std::string style_tag = "\xEE\xB8\x85";
    const std::string preference_tag = "\xEE\xB8\x86";

    auto suffix_bytes = [](const std::string &s, size_t max_bytes) -> std::string
    {
        if (s.size() <= max_bytes)
            return s;
        return s.substr(s.size() - max_bytes);
    };

    std::string conditions;
    if (!profile_utf8.empty())
        conditions += profile_tag + suffix_bytes(profile_utf8, 25);
    if (!topic_utf8.empty())
        conditions += topic_tag + suffix_bytes(topic_utf8, 25);
    if (!style_utf8.empty())
        conditions += style_tag + suffix_bytes(style_utf8, 25);
    if (!preference_utf8.empty())
        conditions += preference_tag + suffix_bytes(preference_utf8, 25);

    std::string left_trim = left_context_utf8;
    if (!left_trim.empty())
    {
        const size_t max_bytes = static_cast<size_t>(std::max(1, max_left_len_chars)) * 4;
        if (left_trim.size() > max_bytes)
        {
            left_trim = left_trim.substr(left_trim.size() - max_bytes);
        }
    }

    const std::string input_kata = hira_to_kata_utf8(input_hira_utf8);

    std::string prompt;
    if (!left_trim.empty())
    {
        prompt = conditions + context_tag + left_trim + input_tag + input_kata + output_tag;
    }
    else
    {
        prompt = conditions + input_tag + input_kata + output_tag;
    }
    return preprocess_text(prompt);
}

static std::vector<llama_token> tokenize(
    const llama_vocab *vocab,
    const std::string &text_utf8,
    bool add_bos,
    bool add_eos)
{
    const int32_t utf8_len = static_cast<int32_t>(text_utf8.size());
    int32_t cap = utf8_len + (add_bos ? 1 : 0) + 8;
    if (cap < 16)
        cap = 16;

    std::vector<llama_token> tmp(static_cast<size_t>(cap));
    const int32_t n = llama_tokenize(
        vocab,
        text_utf8.c_str(),
        utf8_len,
        tmp.data(),
        cap,
        add_bos,
        false);

    std::vector<llama_token> out;
    if (n < 0)
    {
        out.push_back(llama_vocab_bos(vocab));
    }
    else
    {
        out.assign(tmp.begin(), tmp.begin() + n);
    }
    if (add_eos)
        out.push_back(llama_vocab_eos(vocab));
    return out;
}

static void batch_add(
    llama_batch &batch,
    llama_token id,
    llama_pos pos,
    llama_seq_id seq_id,
    bool logits)
{
    const int32_t i = batch.n_tokens;
    batch.token[i] = id;
    batch.pos[i] = pos;
    batch.n_seq_id[i] = 1;
    batch.seq_id[i][0] = seq_id;
    batch.logits[i] = logits ? 1 : 0;
    batch.n_tokens += 1;
}

static const float *get_logits_kv(
    llama_context *ctx,
    llama_seq_id seq_id,
    const std::vector<llama_token> &tokens,
    int logits_start_index,
    KvState &state)
{
    int common = 0;
    const int m = static_cast<int>(std::min(state.prev_tokens.size(), tokens.size()));
    while (common < m && state.prev_tokens[static_cast<size_t>(common)] == tokens[static_cast<size_t>(common)])
    {
        ++common;
    }

    const int prefix_cache = std::min(common, logits_start_index);
    llama_kv_cache_seq_rm(ctx, seq_id, static_cast<llama_pos>(prefix_cache), -1);

    const int32_t need = static_cast<int32_t>(tokens.size() - static_cast<size_t>(prefix_cache));
    if (need <= 0)
    {
        return llama_get_logits(ctx);
    }

    const int32_t cap = std::max<int32_t>(512, need);
    llama_batch batch = llama_batch_init(cap, 0, 1);
    if (!batch.token)
    {
        llama_batch_free(batch);
        return nullptr;
    }

    for (int i = prefix_cache; i < static_cast<int>(tokens.size()); ++i)
    {
        batch_add(batch, tokens[static_cast<size_t>(i)], static_cast<llama_pos>(i), seq_id, i >= logits_start_index);
    }

    const int rc = llama_decode(ctx, batch);
    llama_batch_free(batch);
    if (rc != 0)
    {
        return nullptr;
    }

    state.prev_tokens = tokens;
    return llama_get_logits(ctx);
}

static float logsumexp(const float *logits, int n)
{
    float m = logits[0];
    for (int i = 1; i < n; ++i)
    {
        m = std::max(m, logits[i]);
    }
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        sum += std::exp(static_cast<double>(logits[i]) - static_cast<double>(m));
    }
    return static_cast<float>(static_cast<double>(m) + std::log(sum));
}

static ScoreResult score_candidate_core(
    llama_context *ctx,
    const llama_vocab *vocab,
    const std::vector<llama_token> &prompt_tokens,
    const std::vector<llama_token> &candidate_tokens,
    int skip_prefix_tokens,
    KvState &kv,
    llama_seq_id seq_id)
{
    ScoreResult result;

    std::vector<llama_token> tokens;
    tokens.reserve(prompt_tokens.size() + candidate_tokens.size());
    tokens.insert(tokens.end(), prompt_tokens.begin(), prompt_tokens.end());
    tokens.insert(tokens.end(), candidate_tokens.begin(), candidate_tokens.end());
    if (tokens.size() < 2)
    {
        return result;
    }

    const int start_offset = static_cast<int>(prompt_tokens.size()) - 1;
    if (start_offset < 0)
    {
        return result;
    }

    const float *logits_all = get_logits_kv(ctx, seq_id, tokens, start_offset, kv);
    if (!logits_all)
    {
        return result;
    }

    const int32_t n_vocab = llama_vocab_n_tokens(vocab);
    const size_t score_begin = prompt_tokens.size() + static_cast<size_t>(std::max(0, skip_prefix_tokens));
    auto slice_ptr = [&](int slice_idx) -> const float *
    {
        return logits_all + static_cast<size_t>(slice_idx) * static_cast<size_t>(n_vocab);
    };

    float sum_score = 0.0f;
    int count = 0;
    for (size_t i = prompt_tokens.size(); i < tokens.size(); ++i)
    {
        const int slice_idx = static_cast<int>(i) - 1 - start_offset;
        if (slice_idx < 0)
        {
            continue;
        }
        const float *dist = slice_ptr(slice_idx);

        if (i < score_begin)
        {
            continue;
        }

        const float lse = logsumexp(dist, n_vocab);
        const llama_token actual_tok = tokens[i];
        const float actual_logp = dist[static_cast<int>(actual_tok)] - lse;
        sum_score += actual_logp;
        ++count;
    }

    result.ok = true;
    result.raw_logprob = sum_score;
    result.avg_logprob = count > 0 ? (sum_score / static_cast<float>(count)) : 0.0f;
    result.scored_token_count = count;
    result.skipped_prefix_token_count = std::max(0, skip_prefix_tokens);
    return result;
}

static int common_prefix_token_count(
    const std::vector<llama_token> &lhs,
    const std::vector<llama_token> &rhs)
{
    size_t common = std::min(lhs.size(), rhs.size());
    size_t i = 0;
    while (i < common && lhs[i] == rhs[i])
    {
        ++i;
    }
    return static_cast<int>(i);
}

} // namespace

struct Runner::Impl
{
    KvState kv;
};

Runner::Runner(const Options &opt) : opt_(opt)
{
    if (opt_.threads <= 0)
    {
        opt_.threads = default_threads();
    }

    if (opt_.silence_logs)
    {
        llama_log_set(quiet_llama_log, nullptr);
    }

    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.use_mmap = opt_.use_mmap;
    mparams.n_gpu_layers = opt_.gpu_layers;

    model_ = llama_model_load_from_file(opt_.model_path.c_str(), mparams);
    if (!model_)
    {
        llama_backend_free();
        die("could not load model: " + opt_.model_path);
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = opt_.n_ctx;
    cparams.n_batch = opt_.n_batch;
    cparams.n_threads = opt_.threads;
    cparams.n_threads_batch = opt_.threads;
    cparams.offload_kqv = opt_.offload_kqv;

    ctx_ = llama_init_from_model(model_, cparams);
    if (!ctx_)
    {
        llama_model_free(model_);
        llama_backend_free();
        die("could not init context");
    }

    vocab_ = llama_model_get_vocab(model_);
    if (!vocab_)
    {
        llama_free(ctx_);
        llama_model_free(model_);
        llama_backend_free();
        die("could not get vocab");
    }

    impl_ = new Impl();
}

Runner::~Runner()
{
    delete impl_;
    if (ctx_)
    {
        llama_free(ctx_);
    }
    if (model_)
    {
        llama_model_free(model_);
    }
    llama_backend_free();
}

std::vector<ScoreResult> Runner::score_candidates(
    const std::string &input_hira_utf8,
    const std::vector<std::string> &candidates_utf8,
    ScoreMode score_mode)
{
    std::lock_guard<std::mutex> lock(mu_);

    const std::string prompt = build_prompt_v3(
        opt_.left,
        input_hira_utf8,
        opt_.profile,
        opt_.topic,
        opt_.style,
        opt_.preference,
        40);
    const std::vector<llama_token> prompt_tokens = tokenize(vocab_, prompt, true, false);
    if (prompt_tokens.empty())
    {
        return {};
    }

    std::vector<std::vector<llama_token>> tokenized;
    tokenized.reserve(candidates_utf8.size());
    for (const auto &candidate : candidates_utf8)
    {
        tokenized.push_back(tokenize(vocab_, preprocess_text(candidate), false, false));
    }

    std::vector<int> skip_prefix_counts(tokenized.size(), 0);
    if (score_mode == ScoreMode::DiffTokenSuffix && tokenized.size() > 1)
    {
        const auto &reference = tokenized.front();
        for (size_t i = 1; i < tokenized.size(); ++i)
        {
            skip_prefix_counts[i] = common_prefix_token_count(reference, tokenized[i]);
        }
    }

    std::vector<ScoreResult> out;
    out.reserve(tokenized.size());
    for (size_t i = 0; i < tokenized.size(); ++i)
    {
        llama_kv_cache_seq_rm(ctx_, 0, 0, -1);
        impl_->kv.prev_tokens.clear();
        out.push_back(score_candidate_core(
            ctx_,
            vocab_,
            prompt_tokens,
            tokenized[i],
            std::min(skip_prefix_counts[i], static_cast<int>(tokenized[i].size())),
            impl_->kv,
            0));
    }

    return out;
}

} // namespace kk::zenz
