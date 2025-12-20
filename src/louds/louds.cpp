#include "louds.hpp"
#include <stdexcept>

LOUDS::LOUDS() {
    LBSTemp = {true, false};
    labels  = {U' ', U' '};
    isLeafTemp = {false, false};
}

void LOUDS::convertListToBitVector() {
    BitVector lbs;
    for (bool b : LBSTemp) lbs.push_back(b);
    LBS = std::move(lbs);
    LBSTemp.clear();

    BitVector leaf;
    for (bool b : isLeafTemp) leaf.push_back(b);
    isLeaf = std::move(leaf);
    isLeafTemp.clear();
}

int LOUDS::firstChild(int pos) const {
    const int y = LBS.select0(LBS.rank1(pos)) + 1;
    if (y < 0) return -1;
    return (LBS.get(static_cast<size_t>(y)) ? y : -1);
}

int LOUDS::traverse(int pos, char32_t c) const {
    int childPos = firstChild(pos);
    if (childPos == -1) return -1;

    while (LBS.get(static_cast<size_t>(childPos))) {
        const int labelIndex = LBS.rank1(childPos);
        if (labelIndex >= 0 && static_cast<size_t>(labelIndex) < labels.size()) {
            if (labels[static_cast<size_t>(labelIndex)] == c) return childPos;
        }
        childPos += 1;
    }
    return -1;
}

std::vector<std::u32string> LOUDS::commonPrefixSearch(const std::u32string& str) const {
    std::vector<char32_t> resultTemp;
    std::vector<std::u32string> result;

    int n = 0;
    for (char32_t c : str) {
        n = traverse(n, c);
        if (n == -1) break;

        const int index = LBS.rank1(n);
        if (index < 0 || static_cast<size_t>(index) >= labels.size()) return result;

        resultTemp.push_back(labels[static_cast<size_t>(index)]);

        if (isLeaf.get(static_cast<size_t>(n))) {
            std::u32string tempStr(resultTemp.begin(), resultTemp.end());
            if (!result.empty()) {
                result.push_back(result[0] + tempStr);
            } else {
                result.push_back(tempStr);
                resultTemp.clear();
            }
        }
    }
    return result;
}

bool LOUDS::equals(const LOUDS& other) const {
    return LBS.equals(other.LBS)
        && isLeaf.equals(other.isLeaf)
        && labels == other.labels;
}

void LOUDS::write_u64(std::ostream& os, uint64_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
void LOUDS::write_u32(std::ostream& os, uint32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
void LOUDS::read_u64(std::istream& is, uint64_t& v) {
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
}
void LOUDS::read_u32(std::istream& is, uint32_t& v) {
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
}

void LOUDS::write_u64_vec(std::ostream& os, const std::vector<uint64_t>& v) {
    write_u64(os, static_cast<uint64_t>(v.size()));
    if (!v.empty()) {
        os.write(reinterpret_cast<const char*>(v.data()),
                 static_cast<std::streamsize>(v.size() * sizeof(uint64_t)));
    }
}

std::vector<uint64_t> LOUDS::read_u64_vec(std::istream& is) {
    uint64_t n = 0;
    read_u64(is, n);
    std::vector<uint64_t> v(static_cast<size_t>(n));
    if (n > 0) {
        is.read(reinterpret_cast<char*>(v.data()),
                static_cast<std::streamsize>(n * sizeof(uint64_t)));
    }
    return v;
}

void LOUDS::writeBitVector(std::ostream& os, const BitVector& bv) const {
    write_u64(os, static_cast<uint64_t>(bv.size()));
    write_u64_vec(os, bv.words());
}

BitVector LOUDS::readBitVector(std::istream& is) {
    uint64_t nbits = 0;
    read_u64(is, nbits);
    auto words = read_u64_vec(is);
    BitVector bv;
    bv.assign_from_words(static_cast<size_t>(nbits), std::move(words));
    return bv;
}

void LOUDS::saveToFile(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("failed to open file for write: " + path);

    writeBitVector(ofs, LBS);
    writeBitVector(ofs, isLeaf);

    write_u64(ofs, static_cast<uint64_t>(labels.size()));
    for (char32_t ch : labels) {
        write_u32(ofs, static_cast<uint32_t>(ch));
    }
}

LOUDS LOUDS::loadFromFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("failed to open file for read: " + path);

    LOUDS l;
    l.LBS = readBitVector(ifs);
    l.isLeaf = readBitVector(ifs);

    uint64_t labelN = 0;
    read_u64(ifs, labelN);
    l.labels.resize(static_cast<size_t>(labelN));
    for (size_t i = 0; i < static_cast<size_t>(labelN); ++i) {
        uint32_t v = 0;
        read_u32(ifs, v);
        l.labels[i] = static_cast<char32_t>(v);
    }
    return l;
}
