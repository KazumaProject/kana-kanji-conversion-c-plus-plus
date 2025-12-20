#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>

#include "prefix_with_term_id/prefix_tree_with_term_id.hpp"
#include "louds_with_term_id/converter_with_term_id.hpp"
#include "louds_with_term_id/louds_with_term_id.hpp"

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
    // 1) ASCII: commonPrefixSearch + termId (getTermId) 検証
    // =========================================================
    {
        PrefixTreeWithTermId t;
        t.insert(U"a");   // termId 1
        t.insert(U"ab");  // termId 2
        t.insert(U"abc"); // termId 3

        ConverterWithTermId conv;
        LOUDSWithTermId louds = conv.convert(t.getRoot());

        // commonPrefixSearch は文字列だけ返す
        auto r = louds.commonPrefixSearch(U"abcd");
        std::vector<std::u32string> expected = {U"a", U"ab", U"abc"};
        assert_true(u32_equals(r, expected),
                    "termId: commonPrefixSearch(U\"abcd\") should be {a,ab,abc}");

        // nodeIndex を取って termId を検証（leaf であることを期待）
        const int idx_a = louds.getNodeIndex(U"a");
        const int idx_ab = louds.getNodeIndex(U"ab");
        const int idx_abc = louds.getNodeIndex(U"abc");

        assert_true(idx_a >= 0, "termId: nodeIndex for 'a' should exist");
        assert_true(idx_ab >= 0, "termId: nodeIndex for 'ab' should exist");
        assert_true(idx_abc >= 0, "termId: nodeIndex for 'abc' should exist");

        const int32_t tid_a = louds.getTermId(idx_a);
        const int32_t tid_ab = louds.getTermId(idx_ab);
        const int32_t tid_abc = louds.getTermId(idx_abc);

        assert_true(tid_a == 1, "termId for 'a' should be 1");
        assert_true(tid_ab == 2, "termId for 'ab' should be 2");
        assert_true(tid_abc == 3, "termId for 'abc' should be 3");

        // LOUDS 中身 sanity
        assert_true(louds.LBS.size() > 2, "termId: LBS should be non-trivial");
        assert_true(louds.labels.size() >= 2, "termId: labels should have init elements");
        assert_true(louds.isLeaf.size() == louds.LBS.size(), "termId: isLeaf size should match LBS size");
        assert_true(!louds.termIdsSave.empty(), "termId: termIdsSave should not be empty");
    }

    // =========================================================
    // 2) ひらがな: commonPrefixSearch + termId(getTermId)
    // =========================================================
    {
        PrefixTreeWithTermId ht;
        ht.insert(U"か");     // termId 1
        ht.insert(U"かな");   // termId 2
        ht.insert(U"かなえ"); // termId 3
        ht.insert(U"かなる"); // termId 4

        ConverterWithTermId hconv;
        LOUDSWithTermId hlouds = hconv.convert(ht.getRoot());

        // 入力: 「かなえた」 -> 「か」「かな」「かなえ」
        auto r = hlouds.commonPrefixSearch(U"かなえた");
        std::vector<std::u32string> expected = {U"か", U"かな", U"かなえ"};
        assert_true(u32_equals(r, expected),
                    "termId: hiragana commonPrefixSearch(U\"かなえた\") should be {か,かな,かなえ}");

        // termId 取得確認
        const int idx_ka = hlouds.getNodeIndex(U"か");
        const int idx_kana = hlouds.getNodeIndex(U"かな");
        const int idx_kanae = hlouds.getNodeIndex(U"かなえ");

        assert_true(idx_ka >= 0, "termId: nodeIndex for 'か' should exist");
        assert_true(idx_kana >= 0, "termId: nodeIndex for 'かな' should exist");
        assert_true(idx_kanae >= 0, "termId: nodeIndex for 'かなえ' should exist");

        const int32_t tid_ka = hlouds.getTermId(idx_ka);
        const int32_t tid_kana = hlouds.getTermId(idx_kana);
        const int32_t tid_kanae = hlouds.getTermId(idx_kanae);

        assert_true(tid_ka == 1, "termId for 'か' should be 1");
        assert_true(tid_kana == 2, "termId for 'かな' should be 2");
        assert_true(tid_kanae == 3, "termId for 'かなえ' should be 3");

        // sanity
        assert_true(hlouds.LBS.size() > 2, "termId hiragana: LBS should be non-trivial");
        assert_true(hlouds.labels.size() >= 2, "termId hiragana: labels should have init elements");
        assert_true(hlouds.isLeaf.size() == hlouds.LBS.size(), "termId hiragana: isLeaf size should match LBS size");
    }

    // =========================================================
    // 3) ひらがな: バイナリ round-trip + commonPrefixSearch 一致 + termId 一致
    // =========================================================
    {
        PrefixTreeWithTermId ht;
        ht.insert(U"す");     // termId 1
        ht.insert(U"すみ");   // termId 2
        ht.insert(U"すみれ"); // termId 3

        ConverterWithTermId hconv;
        LOUDSWithTermId hlouds = hconv.convert(ht.getRoot());

        const std::string path = "louds_with_term_id_hiragana_test.bin";
        hlouds.saveToFile(path);

        LOUDSWithTermId loaded = LOUDSWithTermId::loadFromFile(path);

        assert_true(loaded.equals(hlouds),
                    "termId: LOUDSWithTermId binary round-trip should preserve content");

        auto r1 = hlouds.commonPrefixSearch(U"すみれいろ");
        auto r2 = loaded.commonPrefixSearch(U"すみれいろ");
        std::vector<std::u32string> expected = {U"す", U"すみ", U"すみれ"};

        assert_true(u32_equals(r1, expected),
                    "termId: hiragana commonPrefixSearch before save should be {す,すみ,すみれ}");
        assert_true(u32_equals(r2, expected),
                    "termId: hiragana commonPrefixSearch after load should be {す,すみ,すみれ}");

        // termId も一致するか
        const int idx_su = loaded.getNodeIndex(U"す");
        const int idx_sumi = loaded.getNodeIndex(U"すみ");
        const int idx_sumire = loaded.getNodeIndex(U"すみれ");

        assert_true(idx_su >= 0, "termId: loaded nodeIndex for 'す' should exist");
        assert_true(idx_sumi >= 0, "termId: loaded nodeIndex for 'すみ' should exist");
        assert_true(idx_sumire >= 0, "termId: loaded nodeIndex for 'すみれ' should exist");

        assert_true(loaded.getTermId(idx_su) == 1, "termId: loaded termId for 'す' should be 1");
        assert_true(loaded.getTermId(idx_sumi) == 2, "termId: loaded termId for 'すみ' should be 2");
        assert_true(loaded.getTermId(idx_sumire) == 3, "termId: loaded termId for 'すみれ' should be 3");
    }

    std::cout << "[OK] all LOUDSWithTermId tests passed\n";
    return 0;
}
