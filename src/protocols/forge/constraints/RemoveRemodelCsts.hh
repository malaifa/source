// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/forge/constraints/RemoveRemodelCsts.hh
///
/// @brief
/// @author Tom Linsky (tlinsky@uw.edu), Nov 2012

#ifndef INCLUDED_protocols_forge_constraints_RemoveRemodelCsts_hh
#define INCLUDED_protocols_forge_constraints_RemoveRemodelCsts_hh

// Unit Header
#include <protocols/forge/remodel/RemodelConstraintGenerator.hh>

// Package Header

// Proeject Header
#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <protocols/moves/Mover.hh>


#include <utility/vector1.hh>


namespace protocols {
namespace forge {
namespace constraints {

class RemoveRemodelCsts : public protocols::moves::Mover {
public:

	typedef core::Size Size;
	typedef core::Real Real;
	typedef core::pose::Pose Pose;

public:

	RemoveRemodelCsts();

	RemoveRemodelCsts( RemoveRemodelCsts const & rval );

	RemoveRemodelCsts( protocols::forge::remodel::RemodelConstraintGeneratorOP generator );

	virtual ~RemoveRemodelCsts();

	/// @brief this function looks up the constraints created by the object with the given generator and removes them
	virtual void
	apply( core::pose::Pose & pose );

	virtual void
	parse_my_tag( TagCOP tag,
		basic::datacache::DataMap & data,
		protocols::filters::Filters_map const & filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & pose );

	virtual std::string
	get_name() const;

	virtual protocols::moves::MoverOP
	fresh_instance() const;

	virtual protocols::moves::MoverOP
	clone() const;

	void
	set_generator( protocols::forge::remodel::RemodelConstraintGeneratorOP generator );

private:
	protocols::forge::remodel::RemodelConstraintGeneratorOP generator_;
	std::string generator_id_;

}; //class NtoC_RCG


} //namespace constraints
} //namespace forge
} //namespace protocols


#endif // INCLUDED_protocols_forge_constraints_NtoC_RCG_HH
