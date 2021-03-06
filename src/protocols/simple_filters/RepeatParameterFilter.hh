// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/simple_filters/RepeatParameterFilter.hh
/// @brief Simple filter that tests whether a file exists. Useful to test whether we're recovering from a checkpoint
/// @author Sarel Fleishman

#ifndef INCLUDED_protocols_simple_filters_RepeatParameterFilter_hh
#define INCLUDED_protocols_simple_filters_RepeatParameterFilter_hh

//unit headers
#include <protocols/simple_filters/RepeatParameterFilter.fwd.hh>

// Project Headers
#include <core/scoring/ScoreFunction.hh>
#include <core/types.hh>
#include <protocols/filters/Filter.hh>
#include <core/pose/Pose.fwd.hh>
#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <basic/datacache/DataMapObj.hh>

#include <numeric/xyzMatrix.hh>

//external
#include <Eigen/Dense>

namespace protocols {
namespace simple_filters {
// To Author(s) of this code: our coding convention explicitly forbid of using ‘using namespace ...’ in header files outside class or function body, please make sure to refactor this out!
using namespace core;

class RepeatParameterFilter : public filters::Filter
{
public:
	RepeatParameterFilter();
	bool apply( core::pose::Pose const & pose ) const;
	//Real score( core::pose::Pose & pose);
	core::Real report_sm( core::pose::Pose const & pose ) const;
	void report( std::ostream & out,const core::pose::Pose & pose ) const;
	void calculate_helical_parameters( core::pose::Pose const & pose, std::string & handedness, Real & rise_out, Real & radius_out, Real & omega_out) const;
	//void align_to_frame(core::pose::Pose & pose) const; //This seems to have been prototyped but never used.  Removing -- VKM, 19 March 2015.
	void apply_transformation(core::pose::Pose & mod_pose, std::list <core::Size> const & residue_list, numeric::xyzMatrix< core::Real > const & R, numeric::xyzVector< core::Real > const & preT, numeric::xyzVector< core::Real > const & postT) const;
	void matrix3f_to_xyzMatrix( Eigen::Matrix3f const & Re, numeric::xyzMatrix< core::Real> & R  ) const;
	void identity_matrix( numeric::xyzMatrix< core::Real> & R ) const;
	void calculate_helical_parameters_helper( core::pose::Pose const & pose, std::string & handedness, Real & rise_out, Real & radius_out, Real & omega_out) const;
	filters::FilterOP clone() const {
		return filters::FilterOP( new RepeatParameterFilter( *this ) );
	}
	filters::FilterOP fresh_instance() const{
		return filters::FilterOP( new RepeatParameterFilter() );
	}

	virtual ~RepeatParameterFilter();

	void parse_my_tag( utility::tag::TagCOP tag, basic::datacache::DataMap &, protocols::filters::Filters_map const &, protocols::moves::Movers_map const &, core::pose::Pose const & );
private:
	Size numb_repeats_;
	Size startAtRepeat_;
	std::string param_type_;
	Real min_;
	Real max_;
	bool r_handed_;
	bool filter_;
};

}
}

#endif
