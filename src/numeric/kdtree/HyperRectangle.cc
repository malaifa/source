// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file numeric/kdtree/HyperRectangle.cc
/// @brief
/// @author James Thompson

#include <numeric/types.hh>
#include <numeric/kdtree/HyperRectangle.hh>

#include <utility/vector1.hh>
#include <utility/pointer/ReferenceCount.hh>

#include <iostream>

namespace numeric {
namespace kdtree {

/// @details Auto-generated virtual destructor
HyperRectangle::~HyperRectangle() {}

HyperRectangle::HyperRectangle() {}

HyperRectangle::HyperRectangle(
	utility::vector1< utility::vector1< Real > > const & pts
) :
	upper_( pts.front() ),
	lower_( pts.front() )
{
	using utility::vector1;
	typedef vector1< vector1< Real > >::const_iterator iter;
	for ( iter it = pts.begin(), end = pts.end(); it != end; ++it ) {
		extend( *it );
	}
}

HyperRectangle::HyperRectangle(
	utility::vector1< numeric::Real > upper,
	utility::vector1< numeric::Real > lower
)
: upper_( upper ), lower_( lower )
{
	//std::cout << "made bounds: ";
	//show( std::cout );
	//std::cout << std::endl;
}

utility::vector1< numeric::Real > HyperRectangle::upper() const {
	return upper_;
}

utility::vector1< numeric::Real > HyperRectangle::lower() const {
	return lower_;
}

numeric::Size HyperRectangle::ndim() const {
	assert( upper_.size() == lower_.size() );
	return upper().size();
}

HyperRectangle & HyperRectangle::operator = ( HyperRectangle const & src ) {
	upper_ = src.upper();
	lower_ = src.lower();

	return *this;
}

void HyperRectangle::extend(
	utility::vector1< numeric::Real > const & pt
) {
	assert( pt.size() == ndim() );
	using numeric::Real;
	using utility::vector1;

	vector1< Real >::const_iterator p_it = pt.begin(), p_end = pt.end();
	for ( vector1< Real >::iterator
			l_it = lower_.begin(), l_end = lower_.end(),
			u_it = upper_.begin(), u_end = upper_.end();
			p_it != p_end && l_it != l_end && u_it != u_end;
			++p_it, ++l_it, ++u_it
			) {
		*l_it = std::min( *l_it, *p_it );
		*u_it = std::max( *u_it, *p_it );
	}
}

void HyperRectangle::show( std::ostream & out ) const {
	out << "HyperRectangle Lower";
	typedef utility::vector1< Real >::const_iterator iter;
	for ( iter it = lower_.begin(), end = lower_.end(); it != end; ++it ) {
		out << " " << *it;
	}
	out << " Upper ";
	for ( iter it = upper_.begin(), end = upper_.end(); it != end; ++it ) {
		out << " " << *it;
	}
}

} // kdtree
} // numeric
