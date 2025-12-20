#include "louds_with_term_id/converter_with_term_id.hpp"
#include <queue>

LOUDSWithTermId ConverterWithTermId::convert(const PrefixNodeWithTermId *rootNode) const
{
    LOUDSWithTermId louds;

    std::queue<const PrefixNodeWithTermId *> q;
    q.push(rootNode);

    while (!q.empty())
    {
        const PrefixNodeWithTermId *node = q.front();
        q.pop();

        if (node && node->hasChild())
        {
            for (const auto &kv : node->children)
            {
                const char32_t label = kv.first;
                const PrefixNodeWithTermId *child = kv.second.get();

                q.push(child);

                louds.LBSTemp.push_back(true);
                louds.labels.push_back(label);
                louds.isLeafTemp.push_back(child->isWord);

                // Kotlin: if (entry.value.second.isWord) termIds.add(termId)
                if (child->isWord)
                {
                    louds.termIdsSave.push_back(static_cast<int32_t>(child->termId));
                }
            }
        }

        louds.LBSTemp.push_back(false);
        louds.isLeafTemp.push_back(false);
    }

    louds.convertListToBitVector();
    return louds;
}
