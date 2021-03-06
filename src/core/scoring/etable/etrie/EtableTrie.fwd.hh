// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/methods/EtableTrie.fwd.hh
/// @brief  Trie class for etable typedef
/// @author Andrew Leaver-Fay

#ifndef INCLUDED_core_scoring_etable_etrie_EtableTrie_fwd_hh
#define INCLUDED_core_scoring_etable_etrie_EtableTrie_fwd_hh

#include <core/scoring/trie/RotamerTrieBase.fwd.hh>

#include <utility/pointer/owning_ptr.hh>


namespace core {
namespace scoring {
namespace etable {
namespace etrie {

typedef trie::RotamerTrieBaseOP EtableRotamerTrieOP;
typedef trie::RotamerTrieBaseCOP EtableRotamerTrieCOP;

} // etrie
} // etable
} // scoring
} // core

#endif
