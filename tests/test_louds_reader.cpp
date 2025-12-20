#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>

#include "prefix/prefix_tree.hpp"
#include "louds/converter.hpp"
#include "louds/louds.hpp"

#include "louds/louds_reader.hpp"

static void assert_true(bool cond, const char *msg)
{
    if (!cond)
    {
        std::cerr << "[FAIL] " << msg << "\n";
        std::exit(1);
    }
}

static bool u32_equals(const std::vector<std::u32string> &a,
                       const std::vector<std::u32string> &b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

int main()
{
    // =========================================================
    // 1) ASCII: writer->save, reader->load, commonPrefixSearch一致
    // =========================================================
    {
        PrefixTree t;
        t.insert(U"a");
        t.insert(U"ab");
        t.insert(U"abc");

        Converter conv;
        LOUDS louds = conv.convert(t.getRoot());

        const std::string path = "louds_writer_ascii.bin";
        louds.saveToFile(path);

        LOUDSReader reader = LOUDSReader::loadFromFile(path);

        auto r = reader.commonPrefixSearch(U"abcd");
        std::vector<std::u32string> expected = {U"a", U"ab", U"abc"};
        assert_true(u32_equals(r, expected), "reader ASCII commonPrefixSearch should be {a,ab,abc}");

        // getLetter の簡易検証（nodeIndexを探索して文字復元）
        const int idx = reader.getNodeIndex(U"abc");
        assert_true(idx >= 0, "reader getNodeIndex(U\"abc\") should exist");
        const std::u32string s = reader.getLetter(idx);
        assert_true(s == U"abc", "reader getLetter(nodeIndex_of_abc) should be U\"abc\"");
    }

    // =========================================================
    // 2) ひらがな: writer->save, reader->load, commonPrefixSearch一致
    // =========================================================
    {
        PrefixTree t;
        t.insert(U"す");
        t.insert(U"すみ");
        t.insert(U"すみれ");

        Converter conv;
        LOUDS louds = conv.convert(t.getRoot());

        const std::string path = "louds_writer_hira.bin";
        louds.saveToFile(path);

        LOUDSReader reader = LOUDSReader::loadFromFile(path);

        auto r = reader.commonPrefixSearch(U"すみれいろ");
        std::vector<std::u32string> expected = {U"す", U"すみ", U"すみれ"};
        assert_true(u32_equals(r, expected), "reader hiragana commonPrefixSearch should be {す,すみ,すみれ}");

        const int idx = reader.getNodeIndex(U"すみれ");
        assert_true(idx >= 0, "reader getNodeIndex(U\"すみれ\") should exist");
        const std::u32string s = reader.getLetter(idx);
        assert_true(s == U"すみれ", "reader getLetter(nodeIndex_of_すみれ) should be U\"すみれ\"");
    }

    std::cout << "[OK] LOUDSReader tests passed\n";
    return 0;
}
