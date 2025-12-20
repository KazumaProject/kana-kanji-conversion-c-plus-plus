#include "louds/converter.hpp"
#include <queue>

LOUDS Converter::convert(const PrefixNode* rootNode) const {
    LOUDS louds;
    std::queue<const PrefixNode*> q;
    q.push(rootNode);

    while (!q.empty()) {
        const PrefixNode* node = q.front();
        q.pop();

        if (node && node->hasChild()) {
            for (const auto& kv : node->children) {
                const char32_t label = kv.first;
                const PrefixNode* child = kv.second.get();

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
