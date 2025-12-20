#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>

#include "prefix/prefix_tree.hpp"
#include "louds/converter.hpp"
#include "louds/louds.hpp"

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
    // 1) ASCII commonPrefixSearch
    // =========================================================
    {
        PrefixTree t;
        t.insert(U"a");
        t.insert(U"ab");
        t.insert(U"abc");

        Converter conv;
        LOUDS louds = conv.convert(t.getRoot());

        auto r = louds.commonPrefixSearch(U"abcd");
        std::vector<std::u32string> expected = {U"a", U"ab", U"abc"};
        assert_true(u32_equals(r, expected),
                    "commonPrefixSearch(U\"abcd\") should be {a,ab,abc}");

        assert_true(louds.LBS.size() > 2, "LBS should be non-trivial");
        assert_true(louds.labels.size() >= 2, "labels should have init elements");
        assert_true(louds.isLeaf.size() == louds.LBS.size(), "isLeaf size should match LBS size");

        const std::string path = "louds_test.bin";
        louds.saveToFile(path);

        LOUDS loaded = LOUDS::loadFromFile(path);
        assert_true(loaded.equals(louds),
                    "LOUDS binary round-trip should preserve content");

        auto r1 = louds.commonPrefixSearch(U"abcd");
        auto r2 = loaded.commonPrefixSearch(U"abcd");
        assert_true(u32_equals(r1, r2),
                    "commonPrefixSearch should match after load");
    }

    // =========================================================
    // 2) ひらがな commonPrefixSearch
    // =========================================================
    {
        PrefixTree ht;
        ht.insert(U"か");
        ht.insert(U"かな");
        ht.insert(U"かなえ");
        ht.insert(U"かなる");

        Converter hconv;
        LOUDS hlouds = hconv.convert(ht.getRoot());

        auto r = hlouds.commonPrefixSearch(U"かなえた");
        std::vector<std::u32string> expected = {U"か", U"かな", U"かなえ"};
        assert_true(u32_equals(r, expected),
                    "hiragana commonPrefixSearch(U\"かなえた\") should be {か,かな,かなえ}");

        assert_true(hlouds.LBS.size() > 2, "hiragana: LBS should be non-trivial");
        assert_true(hlouds.labels.size() >= 2, "hiragana: labels should have init elements");
        assert_true(hlouds.isLeaf.size() == hlouds.LBS.size(), "hiragana: isLeaf size should match LBS size");
    }

    // =========================================================
    // 3) ひらがな binary round-trip
    // =========================================================
    {
        PrefixTree ht;
        ht.insert(U"す");
        ht.insert(U"すみ");
        ht.insert(U"すみれ");

        Converter hconv;
        LOUDS hlouds = hconv.convert(ht.getRoot());

        const std::string path = "louds_hiragana_test.bin";
        hlouds.saveToFile(path);

        LOUDS loaded = LOUDS::loadFromFile(path);

        assert_true(loaded.equals(hlouds),
                    "hiragana LOUDS binary round-trip should preserve content");

        auto r1 = hlouds.commonPrefixSearch(U"すみれいろ");
        auto r2 = loaded.commonPrefixSearch(U"すみれいろ");
        std::vector<std::u32string> expected = {U"す", U"すみ", U"すみれ"};

        assert_true(u32_equals(r1, expected),
                    "hiragana commonPrefixSearch before save should be {す,すみ,すみれ}");
        assert_true(u32_equals(r2, expected),
                    "hiragana commonPrefixSearch after load should be {す,すみ,すみれ}");
    }

    std::cout << "[OK] all LOUDS tests passed\n";
    return 0;
}
