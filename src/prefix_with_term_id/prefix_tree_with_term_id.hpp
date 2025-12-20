#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <atomic>
#include <cstdint>

struct PrefixNodeWithTermId
{
    char32_t c{U' '};
    int id{-1};
    bool isWord{false};
    int termId{-1};

    std::unordered_map<char32_t, std::unique_ptr<PrefixNodeWithTermId>> children;

    bool hasChild() const { return !children.empty(); }

    PrefixNodeWithTermId *getChild(char32_t ch)
    {
        auto it = children.find(ch);
        return (it == children.end()) ? nullptr : it->second.get();
    }
    const PrefixNodeWithTermId *getChild(char32_t ch) const
    {
        auto it = children.find(ch);
        return (it == children.end()) ? nullptr : it->second.get();
    }

    void addChild(std::unique_ptr<PrefixNodeWithTermId> node)
    {
        const char32_t ch = node->c;
        if (children.find(ch) == children.end())
        {
            children.emplace(ch, std::move(node));
        }
    }
};

class PrefixTreeWithTermId
{
public:
    PrefixTreeWithTermId();

    void insert(const std::u32string &word);

    PrefixNodeWithTermId *getRoot();
    const PrefixNodeWithTermId *getRoot() const;

    int getNodeSize() const;
    int getTermIdSize() const;

private:
    std::unique_ptr<PrefixNodeWithTermId> root;
    std::atomic<int> nextNodeId;
    std::atomic<int> nextTermId;
};
