// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/metal_interface/AddZincSiteConstraints.fwd.hh
/// @brief  AddZincSiteConstraints protocol-mover forward declarations header
/// @author Bryan Der


#ifndef INCLUDED_protocols_metal_interface_AddZincSiteConstraints_FWD_HH
#define INCLUDED_protocols_metal_interface_AddZincSiteConstraints_FWD_HH

// Utility headers
#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace metal_interface {

//Forwards and OP typedefs
class AddZincSiteConstraints;
typedef utility::pointer::shared_ptr< AddZincSiteConstraints > AddZincSiteConstraintsOP;
typedef utility::pointer::shared_ptr< AddZincSiteConstraints const > AddZincSiteConstraintsCOP;

}//metal_interface
}//protocols

#endif //INCLUDED_protocols_metal_interface_AddZincSiteConstraints_FWD_HH
