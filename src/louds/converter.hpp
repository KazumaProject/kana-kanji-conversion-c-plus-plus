#pragma once
#include "prefix/prefix_tree.hpp"
#include "louds/louds.hpp"

class Converter {
public:
    LOUDS convert(const PrefixNode* rootNode) const;
};
