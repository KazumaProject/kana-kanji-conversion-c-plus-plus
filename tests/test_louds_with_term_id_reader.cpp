#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>

#include "prefix_with_term_id/prefix_tree_with_term_id.hpp"
#include "louds_with_term_id/converter_with_term_id.hpp"
#include "louds_with_term_id/louds_with_term_id.hpp"

#include "louds_with_term_id/louds_with_term_id_reader.hpp"

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
    // 1) ASCII: writer->save, reader->load, commonPrefixSearch + termId
    // =========================================================
    {
        PrefixTreeWithTermId t;
        t.insert(U"a");   // termId=1
        t.insert(U"ab");  // termId=2
        t.insert(U"abc"); // termId=3

        ConverterWithTermId conv;
        LOUDSWithTermId louds = conv.convert(t.getRoot());

        const std::string path = "louds_term_writer_ascii.bin";
        louds.saveToFile(path);

        LOUDSWithTermIdReader reader = LOUDSWithTermIdReader::loadFromFile(path);

        auto r = reader.commonPrefixSearch(U"abcd");
        std::vector<std::u32string> expected = {U"a", U"ab", U"abc"};
        assert_true(u32_equals(r, expected), "term reader ASCII commonPrefixSearch should be {a,ab,abc}");

        const int idx_a = reader.getNodeIndex(U"a");
        const int idx_ab = reader.getNodeIndex(U"ab");
        const int idx_abc = reader.getNodeIndex(U"abc");

        assert_true(idx_a >= 0, "term reader nodeIndex 'a' should exist");
        assert_true(idx_ab >= 0, "term reader nodeIndex 'ab' should exist");
        assert_true(idx_abc >= 0, "term reader nodeIndex 'abc' should exist");

        assert_true(reader.getTermId(idx_a) == 1, "term reader termId('a') should be 1");
        assert_true(reader.getTermId(idx_ab) == 2, "term reader termId('ab') should be 2");
        assert_true(reader.getTermId(idx_abc) == 3, "term reader termId('abc') should be 3");
    }

    // =========================================================
    // 2) ひらがな: writer->save, reader->load, commonPrefixSearch + termId
    // =========================================================
    {
        PrefixTreeWithTermId t;
        t.insert(U"す");     // termId=1
        t.insert(U"すみ");   // termId=2
        t.insert(U"すみれ"); // termId=3

        ConverterWithTermId conv;
        LOUDSWithTermId louds = conv.convert(t.getRoot());

        const std::string path = "louds_term_writer_hira.bin";
        louds.saveToFile(path);

        LOUDSWithTermIdReader reader = LOUDSWithTermIdReader::loadFromFile(path);

        auto r = reader.commonPrefixSearch(U"すみれいろ");
        std::vector<std::u32string> expected = {U"す", U"すみ", U"すみれ"};
        assert_true(u32_equals(r, expected), "term reader hiragana commonPrefixSearch should be {す,すみ,すみれ}");

        const int idx_su = reader.getNodeIndex(U"す");
        const int idx_sumi = reader.getNodeIndex(U"すみ");
        const int idx_sumire = reader.getNodeIndex(U"すみれ");

        assert_true(idx_su >= 0, "term reader nodeIndex 'す' should exist");
        assert_true(idx_sumi >= 0, "term reader nodeIndex 'すみ' should exist");
        assert_true(idx_sumire >= 0, "term reader nodeIndex 'すみれ' should exist");

        assert_true(reader.getTermId(idx_su) == 1, "term reader termId('す') should be 1");
        assert_true(reader.getTermId(idx_sumi) == 2, "term reader termId('すみ') should be 2");
        assert_true(reader.getTermId(idx_sumire) == 3, "term reader termId('すみれ') should be 3");
    }

    std::cout << "[OK] LOUDSWithTermIdReader tests passed\n";
    return 0;
}
