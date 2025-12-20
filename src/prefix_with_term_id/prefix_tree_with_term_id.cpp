#include "prefix_tree_with_term_id.hpp"

PrefixTreeWithTermId::PrefixTreeWithTermId()
    : root(std::make_unique<PrefixNodeWithTermId>()),
      nextNodeId(1),
      nextTermId(1) {}

void PrefixTreeWithTermId::insert(const std::u32string &word)
{
    PrefixNodeWithTermId *cur = root.get();

    const int termId = nextTermId.fetch_add(1);

    for (char32_t ch : word)
    {
        if (auto *nxt = cur->getChild(ch))
        {
            cur = nxt;
        }
        else
        {
            int id = nextNodeId.fetch_add(1) + 1;
            auto node = std::make_unique<PrefixNodeWithTermId>();
            node->c = ch;
            node->id = id;
            node->termId = termId;

            PrefixNodeWithTermId *raw = node.get();
            cur->addChild(std::move(node));
            cur = raw;
        }
    }

    cur->isWord = true;
}

PrefixNodeWithTermId *PrefixTreeWithTermId::getRoot() { return root.get(); }
const PrefixNodeWithTermId *PrefixTreeWithTermId::getRoot() const { return root.get(); }

int PrefixTreeWithTermId::getNodeSize() const
{
    return nextNodeId.load();
}

int PrefixTreeWithTermId::getTermIdSize() const
{
    return nextTermId.load();
}
