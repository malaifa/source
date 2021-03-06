// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file PSSM2BfactorMover.fwd.hh
/// @brief switch the chain order
/// @author

#ifndef INCLUDED_protocols_simple_moves_PSSM2BfactorMover_fwd_hh
#define INCLUDED_protocols_simple_moves_PSSM2BfactorMover_fwd_hh

#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace simple_moves {

class PSSM2BfactorMover;
typedef utility::pointer::shared_ptr< PSSM2BfactorMover > PSSM2BfactorMoverOP;
typedef utility::pointer::shared_ptr< PSSM2BfactorMover const > PSSM2BfactorMoverCOP;

} // simple_moves
} // protocols

#endif // INCLUDED_protocols_simple_moves_PSSM2BfactorMover_fwd_hh
