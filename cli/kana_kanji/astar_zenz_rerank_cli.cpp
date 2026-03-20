// cli/kana_kanji/astar_zenz_rerank_cli.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "connection_id/connection_id_builder.hpp"
#include "graph_builder/graph.hpp"
#include "louds/louds_utf16_reader.hpp"
#include "louds_with_term_id/louds_with_term_id_reader_utf16.hpp"
#include "louds_with_term_id/louds_with_term_id_utf16.hpp"
#include "path_algorithm/find_path.hpp"
#include "token_array/token_array.hpp"
#include "zenz/reranker.hpp"
#include "zenz/runner.hpp"

static inline int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static inline int64_t elapsed_ms(int64_t t0_ms)
{
    return now_ms() - t0_ms;
}

static bool utf8_next_codepoint(std::string_view s, size_t &i, char32_t &out_cp)
{
    if (i >= s.size())
        return false;
    const unsigned char c0 = static_cast<unsigned char>(s[i]);

    if (c0 < 0x80)
    {
        out_cp = c0;
        ++i;
        return true;
    }
    if ((c0 & 0xE0) == 0xC0)
    {
        if (i + 1 >= s.size())
            return false;
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c1 & 0xC0) != 0x80)
            return false;
        char32_t cp = c0 & 0x1F;
        cp = (cp << 6) | (c1 & 0x3F);
        if (cp < 0x80)
            return false;
        out_cp = cp;
        i += 2;
        return true;
    }
    if ((c0 & 0xF0) == 0xE0)
    {
        if (i + 2 >= s.size())
            return false;
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
            return false;
        char32_t cp = c0 & 0x0F;
        cp = (cp << 6) | (c1 & 0x3F);
        cp = (cp << 6) | (c2 & 0x3F);
        if (cp < 0x800)
            return false;
        if (cp >= 0xD800 && cp <= 0xDFFF)
            return false;
        out_cp = cp;
        i += 3;
        return true;
    }
    if ((c0 & 0xF8) == 0xF0)
    {
        if (i + 3 >= s.size())
            return false;
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
            return false;
        char32_t cp = c0 & 0x07;
        cp = (cp << 6) | (c1 & 0x3F);
        cp = (cp << 6) | (c2 & 0x3F);
        cp = (cp << 6) | (c3 & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF)
            return false;
        out_cp = cp;
        i += 4;
        return true;
    }
    return false;
}

static bool utf8_to_u16(std::string_view s, std::u16string &out)
{
    out.clear();
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size())
    {
        char32_t cp = 0;
        if (!utf8_next_codepoint(s, i, cp))
            return false;
        if (cp <= 0xFFFF)
        {
            if (cp >= 0xD800 && cp <= 0xDFFF)
                return false;
            out.push_back(static_cast<char16_t>(cp));
        }
        else
        {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + ((cp >> 10) & 0x3FF)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return true;
}

static void append_utf8(std::string &out, char32_t cp)
{
    if (cp <= 0x7F)
        out.push_back(static_cast<char>(cp));
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
}

static bool u16_to_utf8(const std::u16string &s, std::string &out)
{
    out.clear();
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();)
    {
        char32_t cp = 0;
        const char16_t c = s[i++];
        if (c >= 0xD800 && c <= 0xDBFF)
        {
            if (i >= s.size())
                return false;
            const char16_t d = s[i++];
            if (!(d >= 0xDC00 && d <= 0xDFFF))
                return false;
            cp = 0x10000 + (((static_cast<char32_t>(c) - 0xD800) << 10) |
                            (static_cast<char32_t>(d) - 0xDC00));
        }
        else if (c >= 0xDC00 && c <= 0xDFFF)
        {
            return false;
        }
        else
        {
            cp = static_cast<char32_t>(c);
        }
        append_utf8(out, cp);
    }
    return true;
}

static kk::YomiSearchMode parse_yomi_mode(const std::string &s)
{
    if (s == "cps")
        return kk::YomiSearchMode::CommonPrefixOnly;
    if (s == "cps_pred")
        return kk::YomiSearchMode::CommonPrefixPlusPredictive;
    if (s == "cps_omit")
        return kk::YomiSearchMode::CommonPrefixPlusOmission;
    if (s == "all")
        return kk::YomiSearchMode::All;
    throw std::runtime_error("Unknown --yomi_mode: " + s + " (expected: cps|cps_pred|cps_omit|all)");
}

static kk::zenz::RerankMode parse_rerank_mode(const std::string &s)
{
    if (s == "zenz_only")
        return kk::zenz::RerankMode::ZenzOnly;
    if (s == "linear_fuse")
        return kk::zenz::RerankMode::LinearFuse;
    throw std::runtime_error("Unknown --zenz_rerank_mode: " + s + " (expected: zenz_only|linear_fuse)");
}

static kk::zenz::ScoreMode parse_score_mode(const std::string &s)
{
    if (s == "whole")
        return kk::zenz::ScoreMode::WholeCandidate;
    if (s == "diff")
        return kk::zenz::ScoreMode::DiffTokenSuffix;
    throw std::runtime_error("Unknown --zenz_score_mode: " + s + " (expected: whole|diff)");
}

static void usage(const char *argv0)
{
    std::cout
        << "Usage:\n"
        << "  " << argv0
        << " --yomi_termid <yomi_termid.louds> --tango <tango.louds> --tokens <token_array.bin>\n"
        << "      --pos_table <pos_table.bin> --conn <connection_single_column.bin>\n"
        << "      --zenz_model <zenz.gguf> --q <utf8> [--stdin]\n"
        << "      [--n N] [--beam W] [--show_bunsetsu] [--show_yomi] [--show_time] [--time_detail]\n"
        << "      [--yomi_mode cps|cps_pred|cps_omit|all] [--pred_k K]\n"
        << "      [--zenz_rerank_mode zenz_only|linear_fuse] [--zenz_score_mode whole|diff]\n"
        << "      [--zenz_alpha A] [--zenz_beta B] [--zenz_use_raw]\n"
        << "      [--zenz_left \"...\"] [--zenz_profile \"...\"] [--zenz_topic \"...\"] [--zenz_style \"...\"] [--zenz_preference \"...\"]\n"
        << "      [--zenz_n_ctx 512] [--zenz_n_batch 512] [--zenz_threads N] [--zenz_gpu_layers 0] [--zenz_no_mmap]\n"
        << "      [--typo on|off] [--typo_max_penalty N] [--typo_weight W] [--typo_max_out M]\n"
        << "      [--zenz_show_scores] [--verbose]\n";
}

static void run_one(
    const LOUDSReaderUtf16 &yomi_cps,
    const LOUDSWithTermIdReaderUtf16 &yomi_term,
    const TokenArray &tokens,
    const kk::PosTable &pos,
    const LOUDSReaderUtf16 &tango,
    const kk::ConnectionMatrix &conn,
    kk::zenz::Runner &runner,
    const std::string &q_utf8,
    int n_best,
    int beam_width,
    bool show_bunsetsu,
    bool show_yomi,
    bool show_time,
    bool time_detail,
    bool zenz_show_scores,
    kk::YomiSearchMode yomi_mode,
    int pred_k,
    const kk::TypoOptions &typo_opt,
    const kk::zenz::Config &rerank_config)
{
    const int64_t total_t0 = now_ms();

    std::u16string q16;
    if (!utf8_to_u16(q_utf8, q16))
    {
        std::cout << "[BAD_UTF8] " << q_utf8 << "\n";
        return;
    }

    const int64_t astar_t0 = now_ms();
    kk::Graph graph = kk::GraphBuilder::constructGraph(
        q16, yomi_cps, yomi_term, tokens, pos, tango, yomi_mode, pred_k, typo_opt);
    auto ret = kk::FindPath::backwardAStarWithBunsetsu(
        graph,
        static_cast<int>(q16.size()),
        conn,
        n_best,
        beam_width);
    const int64_t astar_ms = elapsed_ms(astar_t0);

    std::vector<kk::zenz::Candidate> candidates;
    candidates.reserve(ret.first.size());
    for (const auto &cand : ret.first)
    {
        kk::zenz::Candidate out;
        if (!u16_to_utf8(cand.string, out.text_utf8))
            out.text_utf8 = "<BAD_U16>";
        if (!u16_to_utf8(cand.yomi, out.yomi_utf8))
            out.yomi_utf8 = "<BAD_U16>";
        out.base_cost = cand.score;
        out.type = static_cast<int>(cand.type);
        out.has_lr = cand.hasLR;
        out.left_id = cand.leftId;
        out.right_id = cand.rightId;
        candidates.push_back(std::move(out));
    }

    const int64_t zenz_t0 = now_ms();
    const auto ranked = kk::zenz::rerank_candidates(runner, candidates, rerank_config);
    const int64_t zenz_ms = elapsed_ms(zenz_t0);
    const int64_t total_ms = elapsed_ms(total_t0);

    std::cout << "query=" << q_utf8
              << " len=" << q16.size()
              << " n=" << n_best
              << " beam=" << beam_width
              << " yomi_mode=" << static_cast<int>(yomi_mode)
              << " pred_k=" << pred_k
              << " zenz_rerank_mode="
              << (rerank_config.rerank_mode == kk::zenz::RerankMode::LinearFuse ? "linear_fuse" : "zenz_only")
              << " zenz_score_mode="
              << (rerank_config.score_mode == kk::zenz::ScoreMode::DiffTokenSuffix ? "diff" : "whole")
              << " zenz_alpha=" << rerank_config.alpha
              << " zenz_beta=" << rerank_config.beta
              << " zenz_use_raw=" << (rerank_config.use_avg_logprob ? "0" : "1")
              << " typo=" << (typo_opt.enable ? "on" : "off");
    if (show_time)
    {
        std::cout << " time_ms=" << total_ms;
        if (time_detail)
        {
            std::cout << " time_ms_astar=" << astar_ms
                      << " time_ms_zenz_eval=" << zenz_ms;
        }
    }
    std::cout << "\n";

    if (show_bunsetsu)
    {
        std::cout << "best_bunsetsu_positions:";
        for (int p : ret.second)
            std::cout << " " << p;
        std::cout << "\n";
    }

    for (size_t i = 0; i < ranked.size(); ++i)
    {
        const auto &cand = ranked[i];
        std::cout << (i + 1) << "\t" << cand.text_utf8
                  << "\tscore=" << cand.base_cost
                  << "\ttype=" << cand.type;
        if (cand.has_lr)
        {
            std::cout << "\tL=" << cand.left_id << "\tR=" << cand.right_id;
        }
        if (zenz_show_scores)
        {
            std::cout << "\tzenz_scored=" << (cand.zenz_scored ? "1" : "0")
                      << "\tzenz_raw=" << cand.zenz_raw_logprob
                      << "\tzenz_avg=" << cand.zenz_avg_logprob
                      << "\tzenz_tok=" << cand.scored_token_count
                      << "\tzenz_skip=" << cand.skipped_prefix_token_count
                      << "\tbase_norm=" << cand.base_score_norm
                      << "\tzenz_norm=" << cand.zenz_score_norm
                      << "\tfused=" << cand.fused_score;
        }
        if (show_yomi)
        {
            std::cout << "\tyomi=" << cand.yomi_utf8;
        }
        std::cout << "\n";
    }
}

int main(int argc, char **argv)
{
    try
    {
        std::string yomi_termid_path;
        std::string tango_path;
        std::string tokens_path;
        std::string pos_path;
        std::string conn_path;
        std::string q;
        bool stdin_mode = false;

        int n_best = 10;
        int beam_width = 50;
        bool show_bunsetsu = false;
        bool show_yomi = false;
        bool show_time = false;
        bool time_detail = false;
        bool zenz_show_scores = false;
        bool verbose = false;

        std::string yomi_mode_str = "cps";
        int pred_k = 1;

        kk::TypoOptions typo_opt;
        typo_opt.enable = false;
        typo_opt.maxPenalty = 3;
        typo_opt.maxOut = 128;
        typo_opt.weight = 1500;

        kk::zenz::Runner::Options runner_opt;
        runner_opt.n_ctx = 512;
        runner_opt.n_batch = 512;
        runner_opt.threads = -1;
        runner_opt.gpu_layers = 0;
        runner_opt.use_mmap = true;
        runner_opt.offload_kqv = false;
        runner_opt.silence_logs = true;

        kk::zenz::Config rerank_config;
        rerank_config.rerank_mode = kk::zenz::RerankMode::LinearFuse;
        rerank_config.score_mode = kk::zenz::ScoreMode::DiffTokenSuffix;
        rerank_config.alpha = 0.35f;
        rerank_config.beta = 1.0f;
        rerank_config.use_avg_logprob = true;

        for (int i = 1; i < argc; ++i)
        {
            const std::string a = argv[i];
            auto need = [&](const char *name) -> std::string
            {
                if (i + 1 >= argc)
                    throw std::runtime_error(std::string("missing value for ") + name);
                return std::string(argv[++i]);
            };

            if (a == "--help" || a == "-h")
            {
                usage(argv[0]);
                return 0;
            }
            if (a == "--yomi_termid")
            {
                yomi_termid_path = need("--yomi_termid");
                continue;
            }
            if (a == "--tango")
            {
                tango_path = need("--tango");
                continue;
            }
            if (a == "--tokens")
            {
                tokens_path = need("--tokens");
                continue;
            }
            if (a == "--pos_table")
            {
                pos_path = need("--pos_table");
                continue;
            }
            if (a == "--conn")
            {
                conn_path = need("--conn");
                continue;
            }
            if (a == "--q")
            {
                q = need("--q");
                continue;
            }
            if (a == "--stdin")
            {
                stdin_mode = true;
                continue;
            }
            if (a == "--n")
            {
                n_best = std::stoi(need("--n"));
                continue;
            }
            if (a == "--beam")
            {
                beam_width = std::stoi(need("--beam"));
                continue;
            }
            if (a == "--show_bunsetsu")
            {
                show_bunsetsu = true;
                continue;
            }
            if (a == "--show_yomi")
            {
                show_yomi = true;
                continue;
            }
            if (a == "--show_time")
            {
                show_time = true;
                continue;
            }
            if (a == "--time_detail")
            {
                time_detail = true;
                continue;
            }
            if (a == "--zenz_show_scores")
            {
                zenz_show_scores = true;
                continue;
            }
            if (a == "--verbose")
            {
                verbose = true;
                runner_opt.verbose = true;
                runner_opt.silence_logs = false;
                continue;
            }
            if (a == "--yomi_mode")
            {
                yomi_mode_str = need("--yomi_mode");
                continue;
            }
            if (a == "--pred_k")
            {
                pred_k = std::stoi(need("--pred_k"));
                continue;
            }
            if (a == "--typo")
            {
                const std::string v = need("--typo");
                if (v == "on")
                    typo_opt.enable = true;
                else if (v == "off")
                    typo_opt.enable = false;
                else
                    throw std::runtime_error("Unknown --typo: " + v + " (expected: on|off)");
                continue;
            }
            if (a == "--typo_max_penalty")
            {
                typo_opt.maxPenalty = std::stoi(need("--typo_max_penalty"));
                continue;
            }
            if (a == "--typo_weight")
            {
                typo_opt.weight = std::stoi(need("--typo_weight"));
                continue;
            }
            if (a == "--typo_max_out")
            {
                typo_opt.maxOut = std::stoi(need("--typo_max_out"));
                continue;
            }
            if (a == "--zenz_model")
            {
                runner_opt.model_path = need("--zenz_model");
                continue;
            }
            if (a == "--zenz_left")
            {
                runner_opt.left = need("--zenz_left");
                continue;
            }
            if (a == "--zenz_profile")
            {
                runner_opt.profile = need("--zenz_profile");
                continue;
            }
            if (a == "--zenz_topic")
            {
                runner_opt.topic = need("--zenz_topic");
                continue;
            }
            if (a == "--zenz_style")
            {
                runner_opt.style = need("--zenz_style");
                continue;
            }
            if (a == "--zenz_preference")
            {
                runner_opt.preference = need("--zenz_preference");
                continue;
            }
            if (a == "--zenz_n_ctx")
            {
                runner_opt.n_ctx = std::stoi(need("--zenz_n_ctx"));
                continue;
            }
            if (a == "--zenz_n_batch")
            {
                runner_opt.n_batch = std::stoi(need("--zenz_n_batch"));
                continue;
            }
            if (a == "--zenz_threads")
            {
                runner_opt.threads = std::stoi(need("--zenz_threads"));
                continue;
            }
            if (a == "--zenz_gpu_layers")
            {
                runner_opt.gpu_layers = std::stoi(need("--zenz_gpu_layers"));
                continue;
            }
            if (a == "--zenz_no_mmap")
            {
                runner_opt.use_mmap = false;
                continue;
            }
            if (a == "--zenz_offload_kqv")
            {
                runner_opt.offload_kqv = true;
                continue;
            }
            if (a == "--zenz_rerank_mode")
            {
                rerank_config.rerank_mode = parse_rerank_mode(need("--zenz_rerank_mode"));
                continue;
            }
            if (a == "--zenz_score_mode")
            {
                rerank_config.score_mode = parse_score_mode(need("--zenz_score_mode"));
                continue;
            }
            if (a == "--zenz_alpha")
            {
                rerank_config.alpha = std::stof(need("--zenz_alpha"));
                continue;
            }
            if (a == "--zenz_beta")
            {
                rerank_config.beta = std::stof(need("--zenz_beta"));
                continue;
            }
            if (a == "--zenz_use_raw")
            {
                rerank_config.use_avg_logprob = false;
                continue;
            }

            throw std::runtime_error("Unknown/incomplete arg: " + a);
        }

        if (yomi_termid_path.empty() || tango_path.empty() || tokens_path.empty() ||
            pos_path.empty() || conn_path.empty() || runner_opt.model_path.empty() ||
            (!stdin_mode && q.empty()))
        {
            usage(argv[0]);
            return 2;
        }

        const kk::YomiSearchMode yomi_mode = parse_yomi_mode(yomi_mode_str);

        const auto yomi_cps = LOUDSReaderUtf16::loadFromFile(yomi_termid_path);
        const auto yomi_trie = LOUDSWithTermIdUtf16::loadFromFile(yomi_termid_path);
        const LOUDSWithTermIdReaderUtf16 yomi_term(yomi_trie);
        const auto tango = LOUDSReaderUtf16::loadFromFile(tango_path);
        const auto tokens = TokenArray::loadFromFile(tokens_path);
        const auto pos = kk::PosTable::loadFromFile(pos_path);
        const auto conn_vec = ConnectionIdBuilder::readShortArrayFromBytesBE(conn_path);
        const kk::ConnectionMatrix conn(std::vector<int16_t>(conn_vec.begin(), conn_vec.end()));

        kk::zenz::Runner runner(runner_opt);

        if (!stdin_mode)
        {
            run_one(
                yomi_cps, yomi_term, tokens, pos, tango, conn, runner, q,
                n_best, beam_width, show_bunsetsu, show_yomi, show_time, time_detail,
                zenz_show_scores, yomi_mode, pred_k, typo_opt, rerank_config);
            return 0;
        }

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            run_one(
                yomi_cps, yomi_term, tokens, pos, tango, conn, runner, line,
                n_best, beam_width, show_bunsetsu, show_yomi, show_time, time_detail,
                zenz_show_scores, yomi_mode, pred_k, typo_opt, rerank_config);
        }

        if (verbose)
        {
            std::cerr << "[done] astar_zenz_rerank_cli finished\n";
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
