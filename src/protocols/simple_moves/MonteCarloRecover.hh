// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/protocols/simple_moves/MonteCarloRecover.hh
/// @author Sarel Fleishman (sarelf@uw.edu)

#ifndef INCLUDED_protocols_moves_MonteCarloRecover_hh
#define INCLUDED_protocols_moves_MonteCarloRecover_hh

// Unit Headers
#include <protocols/simple_moves/MonteCarloRecover.fwd.hh>
#include <protocols/simple_moves/GenericMonteCarloMover.fwd.hh>

// Project Headers
#include <core/types.hh>
#include <core/pose/Pose.fwd.hh>
#include <protocols/filters/Filter.fwd.hh>
#include <protocols/moves/Mover.hh>

// Utility headers

// Parser headers
#include <basic/datacache/DataMap.fwd.hh>
#include <utility/tag/Tag.fwd.hh>

#include <utility/vector1.hh>


namespace protocols {
namespace simple_moves {

class MonteCarloRecover : public protocols::moves::Mover {
public:

	typedef core::Size Size;
	typedef core::Real Real;
	typedef core::pose::Pose Pose;
	typedef core::pose::PoseOP PoseOP;

	typedef utility::tag::TagCOP TagCOP;
	typedef basic::datacache::DataMap DataMap;
	typedef protocols::filters::Filters_map Filters_map;
	typedef protocols::moves::Movers_map Movers_map;
	typedef protocols::moves::MoverOP MoverOP;


public: // constructor/destructor

	/// @brief default constructor
	MonteCarloRecover();

	/// @brief destructor
	~MonteCarloRecover();

	/// @brief create copy constructor
	virtual MoverOP clone() const;

	/// @brief create this type of objectt
	virtual MoverOP fresh_instance() const;
	std::string get_name() const;


	/// @brief apply MonteCarloRecover (Mover)
	virtual void apply( Pose & pose );
	/// @brief set mover
	void set_MC( GenericMonteCarloMoverOP mover );
	GenericMonteCarloMoverOP get_MC() const;
	// Undefinede, commenting out to fix PyRosetta build  void recover( Pose & pose );


	virtual void parse_my_tag(
		TagCOP tag,
		basic::datacache::DataMap & data,
		Filters_map const & filters,
		Movers_map const & movers,
		Pose const &
	);

	bool recover_low() const;
	void recover_low( bool const recover );

private: // data
	bool recover_low_; //dflt true; if false, recovers last

	/// @brief mover
	GenericMonteCarloMoverOP MC_mover_;
};

} // namespace simple_moves
} // namespace protocols

#endif
