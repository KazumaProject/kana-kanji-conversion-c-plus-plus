#include "prefix_tree.hpp"

PrefixTree::PrefixTree() : root(std::make_unique<PrefixNode>()), nextId(1) {}

void PrefixTree::insert(const std::u32string& word) {
    PrefixNode* cur = root.get();
    for (char32_t ch : word) {
        if (auto* nxt = cur->getChild(ch)) {
            cur = nxt;
        } else {
            int id = ++nextId;
            auto node = std::make_unique<PrefixNode>();
            node->c = ch;
            node->id = id;

            PrefixNode* raw = node.get();
            cur->addChild(std::move(node));
            cur = raw;
        }
    }
    cur->isWord = true;
}

PrefixNode* PrefixTree::getRoot() { return root.get(); }
const PrefixNode* PrefixTree::getRoot() const { return root.get(); }
