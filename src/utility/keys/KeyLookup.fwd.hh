// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   utility/keys/KeyLookup.fwd.hh
/// @brief  utility::keys::KeyLookup forward declarations
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)


#ifndef INCLUDED_utility_keys_KeyLookup_fwd_hh
#define INCLUDED_utility_keys_KeyLookup_fwd_hh


namespace utility {
namespace keys {


// Forward
template< typename K > class KeyLookup;


namespace lookup {


// Forward
template< typename K > struct has;
template< typename K > struct key;
template< typename K > struct begin;
template< typename K > struct end;


} // namespace lookup


} // namespace keys
} // namespace utility


#endif // INCLUDED_utility_keys_KeyLookup_FWD_HH