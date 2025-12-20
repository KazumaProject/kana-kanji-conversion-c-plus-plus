#include "louds_with_term_id/louds_with_term_id_reader.hpp"
#include <algorithm>

LOUDSWithTermIdReader::LOUDSWithTermIdReader(const BitVector &lbs,
                                             const BitVector &isLeaf,
                                             std::vector<char32_t> labels,
                                             std::vector<int32_t> termIdsSave)
    : LBS_(lbs),
      isLeaf_(isLeaf),
      labels_(std::move(labels)),
      termIdsSave_(std::move(termIdsSave)),
      lbsSucc_(LBS_),
      leafSucc_(isLeaf_) {}

int LOUDSWithTermIdReader::firstChild(int pos) const
{
    const int y = lbsSucc_.select0(lbsSucc_.rank1(pos)) + 1;
    if (y < 0)
        return -1;
    if (static_cast<size_t>(y) >= LBS_.size())
        return -1;
    return (LBS_.get(static_cast<size_t>(y)) ? y : -1);
}

int LOUDSWithTermIdReader::traverse(int pos, char32_t c) const
{
    int childPos = firstChild(pos);
    if (childPos == -1)
        return -1;

    while (static_cast<size_t>(childPos) < LBS_.size() &&
           LBS_.get(static_cast<size_t>(childPos)))
    {
        const int labelIndex = lbsSucc_.rank1(childPos);
        if (labelIndex >= 0 && static_cast<size_t>(labelIndex) < labels_.size())
        {
            if (labels_[static_cast<size_t>(labelIndex)] == c)
                return childPos;
        }
        childPos += 1;
    }
    return -1;
}

std::vector<std::u32string> LOUDSWithTermIdReader::commonPrefixSearch(const std::u32string &str) const
{
    std::vector<char32_t> resultTemp;
    std::vector<std::u32string> result;

    int n = 0;
    for (char32_t c : str)
    {
        n = traverse(n, c);
        if (n == -1)
            break;

        const int index = lbsSucc_.rank1(n);
        if (index < 0 || static_cast<size_t>(index) >= labels_.size())
            break;

        resultTemp.push_back(labels_[static_cast<size_t>(index)]);

        if (static_cast<size_t>(n) < isLeaf_.size() && isLeaf_.get(static_cast<size_t>(n)))
        {
            std::u32string tempStr(resultTemp.begin(), resultTemp.end());
            if (!result.empty())
            {
                result.push_back(result[0] + tempStr);
            }
            else
            {
                result.push_back(tempStr);
                resultTemp.clear();
            }
        }
    }
    return result;
}

std::u32string LOUDSWithTermIdReader::getLetter(int nodeIndex) const
{
    if (nodeIndex < 0)
        return U"";
    if (static_cast<size_t>(nodeIndex) >= LBS_.size())
        return U"";

    std::u32string out;
    int current = nodeIndex;

    while (true)
    {
        const int nodeId = lbsSucc_.rank1(current);
        if (nodeId < 0 || static_cast<size_t>(nodeId) >= labels_.size())
            break;

        const char32_t ch = labels_[static_cast<size_t>(nodeId)];
        if (ch != U' ')
            out.push_back(ch);

        if (nodeId == 0)
            break;

        const int r0 = lbsSucc_.rank0(current);
        current = lbsSucc_.select1(r0);
        if (current < 0)
            break;
    }

    std::reverse(out.begin(), out.end());
    return out;
}

int LOUDSWithTermIdReader::getNodeIndex(const std::u32string &s) const
{
    return search(2, s, 0);
}

int LOUDSWithTermIdReader::getNodeId(const std::u32string &s) const
{
    const int idx = getNodeIndex(s);
    if (idx < 0)
        return -1;
    return lbsSucc_.rank0(idx);
}

int32_t LOUDSWithTermIdReader::getTermId(int nodeIndex) const
{
    if (nodeIndex < 0)
        return -1;
    if (static_cast<size_t>(nodeIndex) >= isLeaf_.size())
        return -1;

    // leaf でないなら -1
    if (!isLeaf_.get(static_cast<size_t>(nodeIndex)))
        return -1;

    // Kotlin: rank1(nodeIndex) - 1
    const int leafRank = leafSucc_.rank1(nodeIndex);
    const int leafIndex = leafRank - 1;
    if (leafIndex < 0)
        return -1;
    if (static_cast<size_t>(leafIndex) >= termIdsSave_.size())
        return -1;
    return termIdsSave_[static_cast<size_t>(leafIndex)];
}

int LOUDSWithTermIdReader::search(int index, const std::u32string &chars, size_t wordOffset) const
{
    int currentIndex = index;
    if (chars.empty())
        return -1;
    if (currentIndex < 0)
        return -1;

    while (static_cast<size_t>(currentIndex) < LBS_.size() && LBS_.get(static_cast<size_t>(currentIndex)))
    {
        if (wordOffset >= chars.size())
            return currentIndex;

        int charIndex = lbsSucc_.rank1(currentIndex);
        if (charIndex < 0 || static_cast<size_t>(charIndex) >= labels_.size())
            return -1;

        const char32_t currentLabel = labels_[static_cast<size_t>(charIndex)];
        const char32_t currentChar = chars[wordOffset];

        if (currentChar == currentLabel)
        {
            if (wordOffset + 1 == chars.size())
            {
                return currentIndex;
            }
            const int nextIndex = lbsSucc_.select0(charIndex) + 1;
            if (nextIndex < 0)
                return -1;
            return search(nextIndex, chars, wordOffset + 1);
        }

        currentIndex++;
    }
    return -1;
}

void LOUDSWithTermIdReader::write_u64(std::ostream &os, uint64_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermIdReader::write_u32(std::ostream &os, uint32_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermIdReader::write_i32(std::ostream &os, int32_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermIdReader::read_u64(std::istream &is, uint64_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}
void LOUDSWithTermIdReader::read_u32(std::istream &is, uint32_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}
void LOUDSWithTermIdReader::read_i32(std::istream &is, int32_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}

void LOUDSWithTermIdReader::write_u64_vec(std::ostream &os, const std::vector<uint64_t> &v)
{
    write_u64(os, static_cast<uint64_t>(v.size()));
    if (!v.empty())
    {
        os.write(reinterpret_cast<const char *>(v.data()),
                 static_cast<std::streamsize>(v.size() * sizeof(uint64_t)));
    }
}

std::vector<uint64_t> LOUDSWithTermIdReader::read_u64_vec(std::istream &is)
{
    uint64_t n = 0;
    read_u64(is, n);
    std::vector<uint64_t> v(static_cast<size_t>(n));
    if (n > 0)
    {
        is.read(reinterpret_cast<char *>(v.data()),
                static_cast<std::streamsize>(n * sizeof(uint64_t)));
    }
    return v;
}

BitVector LOUDSWithTermIdReader::readBitVector(std::istream &is)
{
    uint64_t nbits = 0;
    read_u64(is, nbits);
    auto words = read_u64_vec(is);
    BitVector bv;
    bv.assign_from_words(static_cast<size_t>(nbits), std::move(words));
    return bv;
}

LOUDSWithTermIdReader LOUDSWithTermIdReader::loadFromFile(const std::string &path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("failed to open file for read: " + path);

    BitVector lbs = readBitVector(ifs);
    BitVector isLeaf = readBitVector(ifs);

    // labels
    uint64_t labelN = 0;
    read_u64(ifs, labelN);

    std::vector<char32_t> labels;
    labels.resize(static_cast<size_t>(labelN));
    for (size_t i = 0; i < static_cast<size_t>(labelN); ++i)
    {
        uint32_t v = 0;
        read_u32(ifs, v);
        labels[i] = static_cast<char32_t>(v);
    }

    // termIdsSave
    uint64_t termN = 0;
    read_u64(ifs, termN);

    std::vector<int32_t> termIds;
    termIds.resize(static_cast<size_t>(termN));
    for (size_t i = 0; i < static_cast<size_t>(termN); ++i)
    {
        int32_t v = 0;
        read_i32(ifs, v);
        termIds[i] = v;
    }

    return LOUDSWithTermIdReader(lbs, isLeaf, std::move(labels), std::move(termIds));
}
