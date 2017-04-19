// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/ContextGraph.fwd.hh
/// @brief  Context graph class forward declaration
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

#ifndef INCLUDED_core_scoring_ContextGraph_fwd_hh
#define INCLUDED_core_scoring_ContextGraph_fwd_hh

// Utility Headers
#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace scoring {

class ContextGraph;

typedef utility::pointer::shared_ptr< ContextGraph > ContextGraphOP;
typedef utility::pointer::shared_ptr< ContextGraph const > ContextGraphCOP;

} // scoring
} // core

#endif