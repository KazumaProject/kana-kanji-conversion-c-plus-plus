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

// 読み込み専用 LOUDS
// - loadFromFile でロード
// - 内部で SuccinctBitVector を構築し rank/select を高速化
class LOUDSReader
{
public:
    LOUDSReader(const BitVector &lbs,
                const BitVector &isLeaf,
                std::vector<char32_t> labels);

    std::vector<std::u32string> commonPrefixSearch(const std::u32string &str) const;

    // Kotlin の getLetter(nodeIndex, succinctBitVector) 相当
    std::u32string getLetter(int nodeIndex) const;

    // Kotlin の getNodeIndex 相当（SuccinctBitVector版）
    int getNodeIndex(const std::u32string &s) const;
    int getNodeId(const std::u32string &s) const;

    const std::vector<char32_t> &getAllLabels() const { return labels_; }

    static LOUDSReader loadFromFile(const std::string &path);

private:
    BitVector LBS_;
    BitVector isLeaf_;
    std::vector<char32_t> labels_;

    SuccinctBitVector lbsSucc_;

    int firstChild(int pos) const;
    int traverse(int pos, char32_t c) const;

    int search(int index, const std::u32string &chars, size_t wordOffset) const;
    int indexOfLabel(int label) const;

    static void write_u64(std::ostream &os, uint64_t v);
    static void write_u32(std::ostream &os, uint32_t v);
    static void read_u64(std::istream &is, uint64_t &v);
    static void read_u32(std::istream &is, uint32_t &v);

    static void write_u64_vec(std::ostream &os, const std::vector<uint64_t> &v);
    static std::vector<uint64_t> read_u64_vec(std::istream &is);

    static BitVector readBitVector(std::istream &is);
};
