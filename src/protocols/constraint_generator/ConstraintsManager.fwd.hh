// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @file protocols/constraint_generator/ConstraintsManager.fwd.hh
/// @brief Manages lists of constraints generated by ConstraintGenerators
/// @author Tom Linsky (tlinsky@uw.edu)


#ifndef INCLUDED_protocols_constraint_generator_ConstraintsManager_fwd_hh
#define INCLUDED_protocols_constraint_generator_ConstraintsManager_fwd_hh

// Utility headers
#include <utility/pointer/owning_ptr.hh>

// Forward
namespace protocols {
namespace constraint_generator {

class ConstraintsMap;

typedef utility::pointer::shared_ptr< ConstraintsMap > ConstraintsMapOP;
typedef utility::pointer::shared_ptr< ConstraintsMap const > ConstraintsMapCOP;

class ConstraintsManager;

typedef utility::pointer::shared_ptr< ConstraintsManager > ConstraintsManagerOP;
typedef utility::pointer::shared_ptr< ConstraintsManager const > ConstraintsManagerCOP;

} //protocols
} //constraint_generator


#endif //INCLUDED_protocols_constraint_generator_ConstraintsManager_fwd_hh
