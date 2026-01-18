// cli/kana_kanji/astar_bunsetsu_parallel_cli.cpp
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <future>
#include <limits>
#include <type_traits>
#include <concepts>

#include "connection_id/connection_id_builder.hpp"
#include "graph_builder/graph.hpp"
#include "louds/louds_utf16_reader.hpp"
#include "louds_with_term_id/louds_with_term_id_reader_utf16.hpp"
#include "louds_with_term_id/louds_with_term_id_utf16.hpp"
#include "path_algorithm/find_path.hpp"
#include "token_array/token_array.hpp"

// -----------------------------
// UTF-8 -> UTF-16 (strict)
// -----------------------------
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
        char32_t cp = (c0 & 0x1F);
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
        char32_t cp = (c0 & 0x0F);
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
        char32_t cp = (c0 & 0x07);
        cp = (cp << 6) | (c1 & 0x3F);
        cp = (cp << 6) | (c2 & 0x3F);
        cp = (cp << 6) | (c3 & 0x3F);
        if (cp < 0x10000)
            return false;
        if (cp > 0x10FFFF)
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

// -----------------------------
// UTF-16 -> UTF-8 (printing)
// -----------------------------
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
        char16_t c = s[i++];

        if (c >= 0xD800 && c <= 0xDBFF)
        {
            if (i >= s.size())
                return false;
            char16_t d = s[i++];
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

// -----------------------------
// Hiragana -> Katakana
// -----------------------------
static std::u16string hira_to_kata(const std::u16string &hira)
{
    std::u16string out;
    out.reserve(hira.size());
    for (char16_t ch : hira)
    {
        if ((ch >= 0x3041 && ch <= 0x3096) || (ch >= 0x309D && ch <= 0x309F))
            out.push_back(static_cast<char16_t>(ch + 0x0060));
        else
            out.push_back(ch);
    }
    return out;
}

static bool starts_with_u16(const std::u16string &s, const std::u16string &prefix)
{
    if (prefix.size() > s.size())
        return false;
    return s.compare(0, prefix.size(), prefix) == 0;
}

// -----------------------------
// Modes
// -----------------------------
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

static void usage(const char *argv0)
{
    std::cout
        << "Usage:\n"
        << "  " << argv0
        << " --yomi_termid <yomi_termid.louds> --tango <tango.louds> --tokens <token_array.bin>\n"
        << "      --pos_table <pos_table.bin> --conn <connection_single_column.bin>\n"
        << "      --q <utf8> [--n N] [--beam W] [--show_bunsetsu]\n"
        << "      [--yomi_mode cps|cps_pred|cps_omit|all]\n"
        << "      [--pred_k K] [--show_pred] [--show_omit]\n"
        << "      [--yomi_n N] [--final_n N] [--no_dedup]\n"
        << "  " << argv0
        << " --yomi_termid <yomi_termid.louds> --tango <tango.louds> --tokens <token_array.bin>\n"
        << "      --pos_table <pos_table.bin> --conn <connection_single_column.bin>\n"
        << "      --stdin [--n N] [--beam W] [--show_bunsetsu]\n"
        << "      [--yomi_mode cps|cps_pred|cps_omit|all]\n"
        << "      [--pred_k K] [--show_pred] [--show_omit]\n"
        << "      [--yomi_n N] [--final_n N] [--no_dedup]\n"
        << "\n"
        << "Notes:\n"
        << "  - commonPrefixSearch candidates are ALWAYS included.\n"
        << "  - --show_pred: also include predictiveSearch-derived candidates.\n"
        << "  - --show_omit: also include omissionSearch-derived candidates.\n"
        << "  - All three (graph/A*, cps, pred, omit) are executed in parallel.\n"
        << "  - If query length is 1 hiragana, limits are auto-disabled (may be huge).\n";
}

// -----------------------------
// Hash for u16string
// -----------------------------
struct U16Hash
{
    size_t operator()(const std::u16string &s) const noexcept
    {
        uint64_t h = 1469598103934665603ULL; // FNV-1a 64-bit
        for (char16_t c : s)
        {
            h ^= static_cast<uint16_t>(c);
            h *= 1099511628211ULL;
        }
        return static_cast<size_t>(h);
    }
};

struct CandidateRow
{
    std::u16string surface;
    std::u16string yomi;      // empty for some graph candidates if unknown
    int score = 0;
    int type = 0;             // graph candidate type or 0 for yomi-derived
    bool hasLR = false;
    int16_t l = 0;
    int16_t r = 0;
    std::string source;       // "graph" | "cps" | "pred" | "omit"
};

// Dedup key (surface + yomi + source + L/R)
struct DedupKey
{
    std::u16string surface;
    std::u16string yomi;
    std::string source;
    int16_t l = 0;
    int16_t r = 0;

    bool operator==(const DedupKey &o) const noexcept
    {
        return surface == o.surface && yomi == o.yomi && source == o.source && l == o.l && r == o.r;
    }
};

struct DedupKeyHash
{
    size_t operator()(const DedupKey &k) const noexcept
    {
        U16Hash h16;
        size_t h = h16(k.surface);
        h ^= (h16(k.yomi) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        // source hash
        std::hash<std::string> hs;
        h ^= (hs(k.source) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        // lr
        h ^= (static_cast<size_t>(static_cast<uint16_t>(k.l)) << 16) ^ static_cast<size_t>(static_cast<uint16_t>(k.r));
        return h;
    }
};

// -----------------------------
// Expand yomi list -> surface candidates via termId/tokens/tango
//  - If dedup=true: keep best (min score) per (surface,yomi,source,L,R)
//  - If query length == 1: do NOT compress by surface; emit all tokens (still optional dedup)
// -----------------------------
static std::vector<CandidateRow> expand_yomi_candidates(
    const std::string &sourceName,
    const std::vector<std::u16string> &yomis,
    const LOUDSWithTermIdReaderUtf16 &yomiTerm,
    const TokenArray &tokens,
    const kk::PosTable &pos,
    const LOUDSReaderUtf16 &tango,
    bool dedup,
    bool queryIsSingleChar,
    size_t yomiLimit // max yomis to process; SIZE_MAX for no limit
)
{
    std::vector<CandidateRow> out;
    if (yomis.empty())
        return out;

    const size_t lim = std::min(yomiLimit, yomis.size());

    if (!dedup)
    {
        // emit all (may be huge)
        for (size_t i = 0; i < lim; ++i)
        {
            const auto &yomi = yomis[i];
            const int32_t termId = yomiTerm.getTermId(yomi);
            if (termId < 0)
                continue;

            const auto listToken = tokens.getTokensForTermId(termId);
            for (const auto &t : listToken)
            {
                CandidateRow row;
                row.source = sourceName;
                row.yomi = yomi;
                row.score = static_cast<int>(t.wordCost);
                row.type = 0;

                std::u16string surface;
                if (t.nodeIndex == TokenArray::HIRAGANA_SENTINEL)
                    surface = yomi;
                else if (t.nodeIndex == TokenArray::KATAKANA_SENTINEL)
                    surface = hira_to_kata(yomi);
                else
                    surface = tango.getLetter(t.nodeIndex);

                row.surface = std::move(surface);

                const auto [l, r] = pos.getLR(t.posIndex);
                row.hasLR = true;
                row.l = l;
                row.r = r;

                out.push_back(std::move(row));
            }
        }
        return out;
    }

    // dedup path
    std::unordered_map<DedupKey, CandidateRow, DedupKeyHash> best;
    best.reserve(4096);

    for (size_t i = 0; i < lim; ++i)
    {
        const auto &yomi = yomis[i];
        const int32_t termId = yomiTerm.getTermId(yomi);
        if (termId < 0)
            continue;

        const auto listToken = tokens.getTokensForTermId(termId);
        for (const auto &t : listToken)
        {
            std::u16string surface;
            if (t.nodeIndex == TokenArray::HIRAGANA_SENTINEL)
                surface = yomi;
            else if (t.nodeIndex == TokenArray::KATAKANA_SENTINEL)
                surface = hira_to_kata(yomi);
            else
                surface = tango.getLetter(t.nodeIndex);

            const auto [l, r] = pos.getLR(t.posIndex);

            DedupKey key;
            key.surface = surface;
            key.yomi = yomi;
            key.source = sourceName;
            key.l = l;
            key.r = r;

            CandidateRow row;
            row.source = sourceName;
            row.surface = std::move(surface);
            row.yomi = yomi;
            row.score = static_cast<int>(t.wordCost);
            row.type = 0;
            row.hasLR = true;
            row.l = l;
            row.r = r;

            // single char: "全部表示"の意図が強いので、dedupはしても「surfaceごとに1つ」ではなく
            // ここでは (surface,yomi,L,R,source) 単位で最良だけに留める（同一キーの重複だけ潰す）。
            auto it = best.find(key);
            if (it == best.end() || row.score < it->second.score)
                best[std::move(key)] = std::move(row);

            (void)queryIsSingleChar; // kept for future policy tweaks
        }
    }

    out.reserve(best.size());
    for (auto &kv : best)
        out.push_back(std::move(kv.second));
    return out;
}

// -----------------------------
// Build yomi lists for each search type
// -----------------------------
static std::vector<std::u16string> get_cps_yomis(const LOUDSReaderUtf16 &yomiCps, const std::u16string &q16)
{
    // Always include
    return yomiCps.commonPrefixSearch(q16);
}

// --- OmissionSearchResult -> u16string extractor (C++20) ---
// commonPrefixSearchWithOmission() returns vector<OmissionSearchResult> (project-defined).
// This extracts the "yomi" string field. If your struct uses a different field name,
// add a new branch here.
template <class T>
static std::u16string extract_u16_yomi(const T &r)
{
    if constexpr (std::is_same_v<T, std::u16string>)
    {
        return r;
    }
    else if constexpr (requires { { r.yomi } -> std::convertible_to<std::u16string>; })
    {
        return static_cast<std::u16string>(r.yomi);
    }
    else if constexpr (requires { { r.text } -> std::convertible_to<std::u16string>; })
    {
        return static_cast<std::u16string>(r.text);
    }
    else if constexpr (requires { { r.prefix } -> std::convertible_to<std::u16string>; })
    {
        return static_cast<std::u16string>(r.prefix);
    }
    else if constexpr (requires { { r.key } -> std::convertible_to<std::u16string>; })
    {
        return static_cast<std::u16string>(r.key);
    }
    else if constexpr (requires { { r.surface } -> std::convertible_to<std::u16string>; })
    {
        return static_cast<std::u16string>(r.surface);
    }
    else
    {
        static_assert(!sizeof(T),
                      "OmissionSearchResult does not expose a u16string field "
                      "(expected one of: yomi/text/prefix/key/surface). "
                      "Update extract_u16_yomi() to match your struct.");
        return {}; // unreachable
    }
}

static std::vector<std::u16string> get_omit_yomis(const LOUDSReaderUtf16 &yomiCps, const std::u16string &q16)
{
    const auto results = yomiCps.commonPrefixSearchWithOmission(q16);

    std::vector<std::u16string> out;
    out.reserve(results.size());
    for (const auto &r : results)
        out.push_back(extract_u16_yomi(r));

    return out;
}

static std::vector<std::u16string> get_pred_yomis(const LOUDSReaderUtf16 &yomiCps, const std::u16string &q16, int predK)
{
    if (q16.empty())
        return {};

    if (predK < 1)
        predK = 1;
    const size_t k = std::min(static_cast<size_t>(predK), q16.size());
    const std::u16string prefix = q16.substr(0, k);

    const auto preds = yomiCps.predictiveSearch(prefix);

    // Keep only yomi that start with q16
    std::vector<std::u16string> out;
    out.reserve(preds.size());
    for (const auto &y : preds)
    {
        if (starts_with_u16(y, q16))
            out.push_back(y);
    }
    return out;
}

// -----------------------------
// Graph + A* (N-best + bunsetsu positions)
// -----------------------------
struct GraphResult
{
    std::vector<CandidateRow> rows;
    std::vector<int> bunsetsu;
};

static GraphResult run_graph_astar(
    const LOUDSReaderUtf16 &yomiCps,
    const LOUDSWithTermIdReaderUtf16 &yomiTerm,
    const TokenArray &tokens,
    const kk::PosTable &pos,
    const LOUDSReaderUtf16 &tango,
    const kk::ConnectionMatrix &conn,
    const std::u16string &q16,
    int nBest,
    int beamWidth,
    kk::YomiSearchMode yomiMode,
    int predK)
{
    GraphResult gr;

    kk::Graph graph = kk::GraphBuilder::constructGraph(q16, yomiCps, yomiTerm, tokens, pos, tango, yomiMode, predK);

    auto [cands, bunsetsu] = kk::FindPath::backwardAStarWithBunsetsu(
        graph,
        static_cast<int>(q16.size()),
        conn,
        nBest,
        beamWidth);

    gr.bunsetsu = std::move(bunsetsu);

    gr.rows.reserve(cands.size());
    for (const auto &c : cands)
    {
        CandidateRow row;
        row.source = "graph";
        row.surface = c.string; // already u16
        row.score = c.score;
        row.type = static_cast<int>(c.type);
        row.hasLR = c.hasLR;
        if (c.hasLR)
        {
            row.l = c.leftId;
            row.r = c.rightId;
        }
        // yomi is not necessarily available in this candidate struct
        gr.rows.push_back(std::move(row));
    }
    return gr;
}

// -----------------------------
// Sorting & printing
// -----------------------------
static void print_rows(
    const std::string &qUtf8,
    const std::u16string &q16,
    const std::vector<int> &bunsetsu,
    bool showBunsetsu,
    std::vector<CandidateRow> rows,
    size_t finalLimit // SIZE_MAX for no limit
)
{
    std::sort(rows.begin(), rows.end(),
              [](const CandidateRow &a, const CandidateRow &b)
              {
                  if (a.score != b.score)
                      return a.score < b.score;
                  if (a.source != b.source)
                      return a.source < b.source;
                  if (a.surface != b.surface)
                      return a.surface < b.surface;
                  return a.yomi < b.yomi;
              });

    std::cout << "query=" << qUtf8 << " len=" << q16.size() << "\n";

    if (showBunsetsu)
    {
        std::cout << "best_bunsetsu_positions:";
        for (int p : bunsetsu)
            std::cout << " " << p;
        std::cout << "\n";
    }

    const size_t n = std::min(finalLimit, rows.size());
    std::cout << "candidates_sorted_by_score: " << n << "/" << rows.size() << "\n";
    for (size_t i = 0; i < n; ++i)
    {
        std::string surf8, y8;
        if (!u16_to_utf8(rows[i].surface, surf8))
            surf8 = "<BAD_U16>";
        if (!rows[i].yomi.empty())
        {
            if (!u16_to_utf8(rows[i].yomi, y8))
                y8 = "<BAD_U16>";
        }
        else
        {
            y8 = "-";
        }

        std::cout << (i + 1) << "\t" << surf8
                  << "\tyomi=" << y8
                  << "\tscore=" << rows[i].score
                  << "\tsrc=" << rows[i].source
                  << "\ttype=" << rows[i].type;

        if (rows[i].hasLR)
            std::cout << "\tL=" << rows[i].l << "\tR=" << rows[i].r;

        std::cout << "\n";
    }
}

// -----------------------------
// One query execution (parallel)
// -----------------------------
static void run_one_parallel(
    const LOUDSReaderUtf16 &yomiCps,
    const LOUDSWithTermIdReaderUtf16 &yomiTerm,
    const TokenArray &tokens,
    const kk::PosTable &pos,
    const LOUDSReaderUtf16 &tango,
    const kk::ConnectionMatrix &conn,
    const std::string &q_utf8,
    int nBest,
    int beamWidth,
    bool showBunsetsu,
    kk::YomiSearchMode yomiMode,
    int predK,
    bool showPred,
    bool showOmit,
    size_t yomiLimit,
    size_t finalLimit,
    bool dedup)
{
    std::u16string q16;
    if (!utf8_to_u16(q_utf8, q16))
    {
        std::cout << "[BAD_UTF8] " << q_utf8 << "\n";
        return;
    }

    const bool queryIsSingleChar = (q16.size() == 1);

    // "1文字は全部表示" ポリシー: 自動で上限解除（ユーザーが明示指定していない限り）
    // ここでは、ユーザーが --yomi_n / --final_n を指定した場合はその値が来る想定。
    // main側でデフォルト値を入れているので、1文字なら解除する。
    if (queryIsSingleChar)
    {
        if (yomiLimit != 0) // 0は「抑止」として扱いたいので解除しない
            yomiLimit = std::numeric_limits<size_t>::max();
        if (finalLimit != 0)
            finalLimit = std::numeric_limits<size_t>::max();
    }

    // 1) parallel tasks
    auto futGraph = std::async(std::launch::async, [&]()
                               { return run_graph_astar(yomiCps, yomiTerm, tokens, pos, tango, conn, q16, nBest, beamWidth, yomiMode, predK); });

    auto futCps = std::async(std::launch::async, [&]()
                             { return get_cps_yomis(yomiCps, q16); });

    std::future<std::vector<std::u16string>> futPred;
    if (showPred)
    {
        futPred = std::async(std::launch::async, [&]()
                             { return get_pred_yomis(yomiCps, q16, predK); });
    }

    std::future<std::vector<std::u16string>> futOmit;
    if (showOmit)
    {
        futOmit = std::async(std::launch::async, [&]()
                             { return get_omit_yomis(yomiCps, q16); });
    }

    // 2) collect results (graph + yomi lists)
    GraphResult gr = futGraph.get();
    const auto cpsYomis = futCps.get();

    std::vector<std::u16string> predYomis;
    if (showPred)
        predYomis = futPred.get();

    std::vector<std::u16string> omitYomis;
    if (showOmit)
        omitYomis = futOmit.get();

    // 3) expand yomi candidates (also parallel)
    auto futCpsRows = std::async(std::launch::async, [&]()
                                 { return expand_yomi_candidates("cps", cpsYomis, yomiTerm, tokens, pos, tango, dedup, queryIsSingleChar, yomiLimit); });

    std::future<std::vector<CandidateRow>> futPredRows;
    if (showPred)
    {
        futPredRows = std::async(std::launch::async, [&]()
                                 { return expand_yomi_candidates("pred", predYomis, yomiTerm, tokens, pos, tango, dedup, queryIsSingleChar, yomiLimit); });
    }

    std::future<std::vector<CandidateRow>> futOmitRows;
    if (showOmit)
    {
        futOmitRows = std::async(std::launch::async, [&]()
                                 { return expand_yomi_candidates("omit", omitYomis, yomiTerm, tokens, pos, tango, dedup, queryIsSingleChar, yomiLimit); });
    }

    auto cpsRows = futCpsRows.get();
    std::vector<CandidateRow> predRows;
    if (showPred)
        predRows = futPredRows.get();

    std::vector<CandidateRow> omitRows;
    if (showOmit)
        omitRows = futOmitRows.get();

    // 4) merge all rows
    std::vector<CandidateRow> all;
    all.reserve(gr.rows.size() + cpsRows.size() + predRows.size() + omitRows.size());

    // graph
    for (auto &r : gr.rows)
        all.push_back(std::move(r));
    // cps always
    for (auto &r : cpsRows)
        all.push_back(std::move(r));
    // optional
    for (auto &r : predRows)
        all.push_back(std::move(r));
    for (auto &r : omitRows)
        all.push_back(std::move(r));

    // optional global dedup across sources? -> 仕様にないので実施しない（見たい情報が消える可能性がある）
    // ただし --no_dedup の場合でも同一行が増えるだけで安全。

    // 5) print
    print_rows(q_utf8, q16, gr.bunsetsu, showBunsetsu, std::move(all),
               (finalLimit == 0 ? 0 : finalLimit));
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

        int nBest = 10;
        int beamWidth = 20;
        bool showBunsetsu = false;

        std::string yomi_mode_str = "cps";
        int predK = 1;

        // New options
        bool showPred = false; // predictive candidates
        bool showOmit = false; // omission candidates

        // limits (0 means suppress printing if used for finalLimit; for yomiLimit, 0 means "process none")
        size_t yomiLimit = 200;  // default cap per yomi-source (auto-disabled for 1 char)
        size_t finalLimit = 200; // default cap for merged output (auto-disabled for 1 char)

        bool dedup = true; // default: reduce trivial duplicates in yomi expansion

        for (int i = 1; i < argc; ++i)
        {
            const std::string a = argv[i];
            if (a == "--help" || a == "-h")
            {
                usage(argv[0]);
                return 0;
            }
            if (a == "--yomi_termid" && i + 1 < argc)
            {
                yomi_termid_path = argv[++i];
                continue;
            }
            if (a == "--tango" && i + 1 < argc)
            {
                tango_path = argv[++i];
                continue;
            }
            if (a == "--tokens" && i + 1 < argc)
            {
                tokens_path = argv[++i];
                continue;
            }
            if (a == "--pos_table" && i + 1 < argc)
            {
                pos_path = argv[++i];
                continue;
            }
            if (a == "--conn" && i + 1 < argc)
            {
                conn_path = argv[++i];
                continue;
            }
            if (a == "--q" && i + 1 < argc)
            {
                q = argv[++i];
                continue;
            }
            if (a == "--stdin")
            {
                stdin_mode = true;
                continue;
            }
            if (a == "--n" && i + 1 < argc)
            {
                nBest = std::stoi(argv[++i]);
                continue;
            }
            if (a == "--beam" && i + 1 < argc)
            {
                beamWidth = std::stoi(argv[++i]);
                continue;
            }
            if (a == "--show_bunsetsu")
            {
                showBunsetsu = true;
                continue;
            }
            if (a == "--yomi_mode" && i + 1 < argc)
            {
                yomi_mode_str = argv[++i];
                continue;
            }
            if (a == "--pred_k" && i + 1 < argc)
            {
                predK = std::stoi(argv[++i]);
                continue;
            }

            // new
            if (a == "--show_pred")
            {
                showPred = true;
                continue;
            }
            if (a == "--show_omit")
            {
                showOmit = true;
                continue;
            }
            if (a == "--yomi_n" && i + 1 < argc)
            {
                yomiLimit = static_cast<size_t>(std::stoul(argv[++i]));
                continue;
            }
            if (a == "--final_n" && i + 1 < argc)
            {
                finalLimit = static_cast<size_t>(std::stoul(argv[++i]));
                continue;
            }
            if (a == "--no_dedup")
            {
                dedup = false;
                continue;
            }

            throw std::runtime_error("Unknown/incomplete arg: " + a);
        }

        if (yomi_termid_path.empty() || tango_path.empty() || tokens_path.empty() ||
            pos_path.empty() || conn_path.empty() ||
            (!stdin_mode && q.empty()))
        {
            usage(argv[0]);
            return 2;
        }

        const kk::YomiSearchMode yomiMode = parse_yomi_mode(yomi_mode_str);

        // load resources
        const auto yomiCps = LOUDSReaderUtf16::loadFromFile(yomi_termid_path);
        const auto yomiTrie = LOUDSWithTermIdUtf16::loadFromFile(yomi_termid_path);
        const LOUDSWithTermIdReaderUtf16 yomiTerm(yomiTrie);

        const auto tango = LOUDSReaderUtf16::loadFromFile(tango_path);
        const auto tokens = TokenArray::loadFromFile(tokens_path);
        const auto pos = kk::PosTable::loadFromFile(pos_path);

        const auto connVec = ConnectionIdBuilder::readShortArrayFromBytesBE(conn_path);
        const kk::ConnectionMatrix conn(std::vector<int16_t>(connVec.begin(), connVec.end()));

        if (!stdin_mode)
        {
            run_one_parallel(
                yomiCps, yomiTerm, tokens, pos, tango, conn,
                q, nBest, beamWidth, showBunsetsu,
                yomiMode, predK,
                showPred, showOmit,
                yomiLimit, finalLimit,
                dedup);
            return 0;
        }

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            run_one_parallel(
                yomiCps, yomiTerm, tokens, pos, tango, conn,
                line, nBest, beamWidth, showBunsetsu,
                yomiMode, predK,
                showPred, showOmit,
                yomiLimit, finalLimit,
                dedup);
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
