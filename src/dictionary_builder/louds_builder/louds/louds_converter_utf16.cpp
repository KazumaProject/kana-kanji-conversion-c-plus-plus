#include "louds/louds_converter_utf16.hpp"
#include <queue>

LOUDSUtf16 ConverterUtf16::convert(const PrefixNodeUtf16 *rootNode) const
{
    LOUDSUtf16 louds;
    std::queue<const PrefixNodeUtf16 *> q;
    q.push(rootNode);

    while (!q.empty())
    {
        const PrefixNodeUtf16 *node = q.front();
        q.pop();

        if (node && node->hasChild())
        {
            for (const auto &kv : node->children)
            {
                const char16_t label = kv.first;
                const PrefixNodeUtf16 *child = kv.second.get();

                q.push(child);
                louds.LBSTemp.push_back(true);
                louds.labels.push_back(label);
                louds.isLeafTemp.push_back(child->isWord);
            }
        }

        louds.LBSTemp.push_back(false);
        louds.isLeafTemp.push_back(false);
    }

    louds.convertListToBitVector();
    return louds;
}
