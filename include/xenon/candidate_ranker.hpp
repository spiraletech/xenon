#pragma once

#include "xenon/candidate_pool.hpp"

namespace xenon {

class CandidateRanker {
public:
    void rank(CandidatePool& pool) const;
};

} // namespace xenon
