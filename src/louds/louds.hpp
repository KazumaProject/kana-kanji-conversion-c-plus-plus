#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <istream>
#include "common/bit_vector.hpp"

class LOUDS {
public:
    std::vector<bool> LBSTemp;
    std::vector<bool> isLeafTemp;

    BitVector LBS;
    BitVector isLeaf;
    std::vector<char32_t> labels;

    LOUDS();

    void convertListToBitVector();

    std::vector<std::u32string> commonPrefixSearch(const std::u32string& str) const;

    void saveToFile(const std::string& path) const;
    static LOUDS loadFromFile(const std::string& path);

    bool equals(const LOUDS& other) const;

private:
    int firstChild(int pos) const;
    int traverse(int pos, char32_t c) const;

    static void write_u64(std::ostream& os, uint64_t v);
    static void write_u32(std::ostream& os, uint32_t v);
    static void write_u64_vec(std::ostream& os, const std::vector<uint64_t>& v);
    static void read_u64(std::istream& is, uint64_t& v);
    static void read_u32(std::istream& is, uint32_t& v);
    static std::vector<uint64_t> read_u64_vec(std::istream& is);

    void writeBitVector(std::ostream& os, const BitVector& bv) const;
    static BitVector readBitVector(std::istream& is);
};
