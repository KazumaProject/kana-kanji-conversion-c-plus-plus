#include "louds_with_term_id.hpp"

LOUDSWithTermId::LOUDSWithTermId()
{
    // Kotlin の init と同じ初期状態
    LBSTemp = {true, false};
    labels = {U' ', U' '};
    isLeafTemp = {false, false};
    termIdsSave.clear();
}

void LOUDSWithTermId::convertListToBitVector()
{
    BitVector lbs;
    for (bool b : LBSTemp)
        lbs.push_back(b);
    LBS = std::move(lbs);
    LBSTemp.clear();

    BitVector leaf;
    for (bool b : isLeafTemp)
        leaf.push_back(b);
    isLeaf = std::move(leaf);
    isLeafTemp.clear();
}

int LOUDSWithTermId::firstChild(int pos) const
{
    const int y = LBS.select0(LBS.rank1(pos)) + 1;
    if (y < 0)
        return -1;
    if (static_cast<size_t>(y) >= LBS.size())
        return -1;
    return (LBS.get(static_cast<size_t>(y)) ? y : -1);
}

int LOUDSWithTermId::traverse(int pos, char32_t c) const
{
    int childPos = firstChild(pos);
    if (childPos == -1)
        return -1;

    while (static_cast<size_t>(childPos) < LBS.size() &&
           LBS.get(static_cast<size_t>(childPos)))
    {
        const int labelIndex = LBS.rank1(childPos);
        if (labelIndex >= 0 && static_cast<size_t>(labelIndex) < labels.size())
        {
            if (labels[static_cast<size_t>(labelIndex)] == c)
                return childPos;
        }
        childPos += 1;
    }
    return -1;
}

std::vector<std::u32string> LOUDSWithTermId::commonPrefixSearch(const std::u32string &str) const
{
    std::vector<char32_t> resultTemp;
    std::vector<std::u32string> result;

    int n = 0;
    for (char32_t c : str)
    {
        n = traverse(n, c);
        if (n == -1)
            break;

        const int index = LBS.rank1(n);
        if (index < 0 || static_cast<size_t>(index) >= labels.size())
            return result;

        resultTemp.push_back(labels[static_cast<size_t>(index)]);

        if (static_cast<size_t>(n) < isLeaf.size() && isLeaf.get(static_cast<size_t>(n)))
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

int32_t LOUDSWithTermId::getTermId(int nodeIndex) const
{
    if (nodeIndex < 0)
        return -1;
    if (static_cast<size_t>(nodeIndex) >= isLeaf.size())
        return -1;

    // Kotlin 実装を安全化: leaf でないなら -1
    if (!isLeaf.get(static_cast<size_t>(nodeIndex)))
        return -1;

    // Kotlin: val firstNodeId = isLeaf.rank1(nodeIndex) - 1
    const int leafRank = isLeaf.rank1(nodeIndex); // [0..nodeIndex] の leaf 数
    const int leafIndex = leafRank - 1;           // 0-based
    if (leafIndex < 0)
        return -1;
    if (static_cast<size_t>(leafIndex) >= termIdsSave.size())
        return -1;
    return termIdsSave[static_cast<size_t>(leafIndex)];
}

int LOUDSWithTermId::getNodeIndex(const std::u32string &s) const
{
    return search(2, s, 0);
}

int LOUDSWithTermId::getNodeId(const std::u32string &s) const
{
    const int idx = getNodeIndex(s);
    if (idx < 0)
        return -1;
    return LBS.rank0(idx);
}

int LOUDSWithTermId::search(int index, const std::u32string &chars, size_t wordOffset) const
{
    int index2 = index;
    size_t wordOffset2 = wordOffset;

    if (chars.empty())
        return -1;
    if (index2 < 0)
        return -1;

    while (static_cast<size_t>(index2) < LBS.size() && LBS.get(static_cast<size_t>(index2)))
    {
        if (wordOffset2 >= chars.size())
            return index2;

        int charIndex = LBS.rank1(index2);
        if (charIndex < 0 || static_cast<size_t>(charIndex) >= labels.size())
            return -1;

        if (chars[wordOffset2] == labels[static_cast<size_t>(charIndex)])
        {
            // 末尾に到達していたらそこで終了
            if (wordOffset2 + 1 == chars.size())
            {
                return index2;
            }
            // 次の階層へ
            const int nextIndex = indexOfLabel(charIndex);
            if (nextIndex < 0)
                return -1;
            return search(nextIndex, chars, wordOffset2 + 1);
        }
        else
        {
            index2++;
        }
    }
    return -1;
}

int LOUDSWithTermId::indexOfLabel(int label) const
{
    int count = 0;
    for (size_t i = 0; i < LBS.size(); ++i)
    {
        if (!LBS.get(i))
        {
            ++count;
            if (count == label)
            {
                // Kotlin: return i + 1
                if (i + 1 >= LBS.size())
                    return -1;
                return static_cast<int>(i + 1);
            }
        }
    }
    return -1;
}

bool LOUDSWithTermId::equals(const LOUDSWithTermId &other) const
{
    return LBS.equals(other.LBS) && isLeaf.equals(other.isLeaf) && labels == other.labels && termIdsSave == other.termIdsSave;
}

void LOUDSWithTermId::write_u64(std::ostream &os, uint64_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermId::write_u32(std::ostream &os, uint32_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermId::write_i32(std::ostream &os, int32_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
void LOUDSWithTermId::read_u64(std::istream &is, uint64_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}
void LOUDSWithTermId::read_u32(std::istream &is, uint32_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}
void LOUDSWithTermId::read_i32(std::istream &is, int32_t &v)
{
    is.read(reinterpret_cast<char *>(&v), sizeof(v));
}

void LOUDSWithTermId::write_u64_vec(std::ostream &os, const std::vector<uint64_t> &v)
{
    write_u64(os, static_cast<uint64_t>(v.size()));
    if (!v.empty())
    {
        os.write(reinterpret_cast<const char *>(v.data()),
                 static_cast<std::streamsize>(v.size() * sizeof(uint64_t)));
    }
}

std::vector<uint64_t> LOUDSWithTermId::read_u64_vec(std::istream &is)
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

void LOUDSWithTermId::writeBitVector(std::ostream &os, const BitVector &bv) const
{
    write_u64(os, static_cast<uint64_t>(bv.size()));
    write_u64_vec(os, bv.words());
}

BitVector LOUDSWithTermId::readBitVector(std::istream &is)
{
    uint64_t nbits = 0;
    read_u64(is, nbits);
    auto words = read_u64_vec(is);
    BitVector bv;
    bv.assign_from_words(static_cast<size_t>(nbits), std::move(words));
    return bv;
}

void LOUDSWithTermId::saveToFile(const std::string &path) const
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        throw std::runtime_error("failed to open file for write: " + path);

    // 1) BitVectors
    writeBitVector(ofs, LBS);
    writeBitVector(ofs, isLeaf);

    // 2) labels
    write_u64(ofs, static_cast<uint64_t>(labels.size()));
    for (char32_t ch : labels)
    {
        write_u32(ofs, static_cast<uint32_t>(ch));
    }

    // 3) termIdsSave
    write_u64(ofs, static_cast<uint64_t>(termIdsSave.size()));
    for (int32_t tid : termIdsSave)
    {
        write_i32(ofs, tid);
    }
}

LOUDSWithTermId LOUDSWithTermId::loadFromFile(const std::string &path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("failed to open file for read: " + path);

    LOUDSWithTermId l;

    // 1) BitVectors
    l.LBS = readBitVector(ifs);
    l.isLeaf = readBitVector(ifs);

    // 2) labels
    uint64_t labelN = 0;
    read_u64(ifs, labelN);
    l.labels.resize(static_cast<size_t>(labelN));
    for (size_t i = 0; i < static_cast<size_t>(labelN); ++i)
    {
        uint32_t v = 0;
        read_u32(ifs, v);
        l.labels[i] = static_cast<char32_t>(v);
    }

    // 3) termIdsSave
    uint64_t termN = 0;
    read_u64(ifs, termN);
    l.termIdsSave.resize(static_cast<size_t>(termN));
    for (size_t i = 0; i < static_cast<size_t>(termN); ++i)
    {
        int32_t v = 0;
        read_i32(ifs, v);
        l.termIdsSave[i] = v;
    }

    return l;
}
