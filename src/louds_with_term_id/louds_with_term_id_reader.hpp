#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <ostream>
#include <istream>

#include "common/bit_vector.hpp"
#include "common/succinct_bit_vector.hpp"

// 読み込み専用 LOUDSWithTermId
// - commonPrefixSearch は文字列のみ返す（ユーザー要件どおり）
// - termId は getTermId(nodeIndex) で別途取得
class LOUDSWithTermIdReader
{
public:
    LOUDSWithTermIdReader(const BitVector &lbs,
                          const BitVector &isLeaf,
                          std::vector<char32_t> labels,
                          std::vector<int32_t> termIdsSave);

    std::vector<std::u32string> commonPrefixSearch(const std::u32string &str) const;

    std::u32string getLetter(int nodeIndex) const;

    int getNodeIndex(const std::u32string &s) const;
    int getNodeId(const std::u32string &s) const;

    // leaf の nodeIndex を渡す想定
    int32_t getTermId(int nodeIndex) const;

    static LOUDSWithTermIdReader loadFromFile(const std::string &path);

private:
    BitVector LBS_;
    BitVector isLeaf_;
    std::vector<char32_t> labels_;
    std::vector<int32_t> termIdsSave_;

    SuccinctBitVector lbsSucc_;
    SuccinctBitVector leafSucc_;

    int firstChild(int pos) const;
    int traverse(int pos, char32_t c) const;

    int search(int index, const std::u32string &chars, size_t wordOffset) const;

    static void write_u64(std::ostream &os, uint64_t v);
    static void write_u32(std::ostream &os, uint32_t v);
    static void write_i32(std::ostream &os, int32_t v);

    static void read_u64(std::istream &is, uint64_t &v);
    static void read_u32(std::istream &is, uint32_t &v);
    static void read_i32(std::istream &is, int32_t &v);

    static void write_u64_vec(std::ostream &os, const std::vector<uint64_t> &v);
    static std::vector<uint64_t> read_u64_vec(std::istream &is);

    static BitVector readBitVector(std::istream &is);
};
