#pragma once
#include "prefix_with_term_id/prefix_tree_with_term_id.hpp"
#include "louds_with_term_id/louds_with_term_id.hpp"

class ConverterWithTermId
{
public:
    LOUDSWithTermId convert(const PrefixNodeWithTermId *rootNode) const;
};
