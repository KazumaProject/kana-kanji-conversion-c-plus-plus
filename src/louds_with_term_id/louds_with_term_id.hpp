#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <istream>
#include <stdexcept>

#include "common/bit_vector.hpp"

class LOUDSWithTermId
{
public:
    // builder 用（BFS で push して最後に BitVector 化）
    std::vector<bool> LBSTemp;
    std::vector<bool> isLeafTemp;

    // LOUDS 本体
    BitVector LBS;
    BitVector isLeaf;
    std::vector<char32_t> labels;

    // leaf ノードに対応する termId 配列（leaf の出現順）
    // Kotlin: termIdsSave (IntArray)
    std::vector<int32_t> termIdsSave;

    LOUDSWithTermId();

    void convertListToBitVector();

    // termId を同時に返さない（ユーザー要件）
    std::vector<std::u32string> commonPrefixSearch(const std::u32string &str) const;

    // termId 取得（leaf nodeIndex に対して使う想定）
    int32_t getTermId(int nodeIndex) const;

    // （テストや利用のため）ノード探索 API も用意（Kotlin 相当）
    int getNodeIndex(const std::u32string &s) const;
    int getNodeId(const std::u32string &s) const;

    void saveToFile(const std::string &path) const;
    static LOUDSWithTermId loadFromFile(const std::string &path);

    bool equals(const LOUDSWithTermId &other) const;

private:
    int firstChild(int pos) const;
    int traverse(int pos, char32_t c) const;

    // Kotlin: search / indexOfLabel 相当
    int search(int index, const std::u32string &chars, size_t wordOffset) const;
    int indexOfLabel(int label) const;

    static void write_u64(std::ostream &os, uint64_t v);
    static void write_u32(std::ostream &os, uint32_t v);
    static void write_i32(std::ostream &os, int32_t v);

    static void read_u64(std::istream &is, uint64_t &v);
    static void read_u32(std::istream &is, uint32_t &v);
    static void read_i32(std::istream &is, int32_t &v);

    static void write_u64_vec(std::ostream &os, const std::vector<uint64_t> &v);
    static std::vector<uint64_t> read_u64_vec(std::istream &is);

    void writeBitVector(std::ostream &os, const BitVector &bv) const;
    static BitVector readBitVector(std::istream &is);
};
