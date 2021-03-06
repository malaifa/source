// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @file protocols/simple_filters/ConstraintScoreFilter.hh
/// @brief Filter that computes scores of constraints generated by ConstraintGenerators
/// @author Tom Linsky (tlinsky@uw.edu)

#ifndef INCLUDED_protocols_simple_filters_ConstraintScoreFilter_hh
#define INCLUDED_protocols_simple_filters_ConstraintScoreFilter_hh

// Unit headers
#include <protocols/simple_filters/ConstraintScoreFilter.fwd.hh>
#include <protocols/filters/Filter.hh>

// Protocol headers
#include <protocols/constraint_generator/ConstraintGenerator.fwd.hh>

// Core headers
#include <core/pose/Pose.fwd.hh>

// Basic/Utility headers
#include <basic/datacache/DataMap.fwd.hh>

namespace protocols {
namespace simple_filters {

///@brief Filter that computes scores of constraints generated by ConstraintGenerators
class ConstraintScoreFilter : public protocols::filters::Filter {
	typedef utility::vector1< constraint_generator::ConstraintGeneratorCOP > ConstraintGeneratorCOPs;

public:
	ConstraintScoreFilter();

	// destructor (important for properly forward-declaring smart-pointer members)
	virtual ~ConstraintScoreFilter();

	/// @brief returns true if the structure passes the filter, false otherwise
	virtual bool
	apply( core::pose::Pose const & pose ) const;

	/// @brief required for reporting score values
	virtual core::Real
	report_sm( core::pose::Pose const & pose ) const;

	/// @brief allows printing data to a stream
	virtual void
	report( std::ostream & os, core::pose::Pose const & pose ) const;

public:
	virtual std::string
	get_name() const;

	/// @brief parse XML tag (to use this Mover in Rosetta Scripts)
	virtual void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data,
		protocols::filters::Filters_map const & filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & pose );

	/// @brief required in the context of the parser/scripting scheme
	virtual protocols::filters::FilterOP
	fresh_instance() const;

	/// @brief required in the context of the parser/scripting scheme
	virtual protocols::filters::FilterOP
	clone() const;

private:
	ConstraintGeneratorCOPs cgs_;
	core::Real threshold_;

};

} //protocols
} //simple_filters

#endif //INCLUDED_protocols_simple_filters_ConstraintScoreFilter_hh
