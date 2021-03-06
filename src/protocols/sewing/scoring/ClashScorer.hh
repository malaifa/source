// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file ClashScorer.hh
///
/// @brief Identifies and scores backbone clashes in SEWING Assemblies
/// @author Tim Jacobs

#ifndef INCLUDED_protocols_sewing_scoring_ClashScorer_hh
#define INCLUDED_protocols_sewing_scoring_ClashScorer_hh

//Unit headers
#include <protocols/sewing/scoring/ClashScorer.fwd.hh>
#include <protocols/sewing/scoring/AssemblyScorer.hh>

//Package headers
#include <protocols/sewing/conformation/Assembly.hh>

//Utility headers
#include <utility/vector1.hh>

#include <utility/tag/Tag.hh>

namespace protocols {
namespace sewing  {
namespace scoring {

class ClashScorer : public AssemblyScorer {

public:

	///@brief default construct
	ClashScorer();

	virtual ~ClashScorer(){}

	virtual
	core::Real
	score(
		AssemblyCOP assembly
	);
};


} //scoring namespace
} //sewing namespace
} //protocols namespace

#endif
