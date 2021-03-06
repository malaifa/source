// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @file utility/util.cc
/// @brief Utility functions for std::maps
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)


namespace utility {

template < class T >
bool
has_key(std::map< T, T > a_map, T key){
	if (a_map.count(key) > 0){
		return true;
	}
	else {
		return false;
	}
}




} //utility


