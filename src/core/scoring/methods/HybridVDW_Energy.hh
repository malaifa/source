// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/methods/HybridVDW_Energy.hh
/// @brief  Statistically derived rotamer pair potential class declaration
/// @author Phil Bradley


#ifndef INCLUDED_core_scoring_methods_HybridVDW_Energy_hh
#define INCLUDED_core_scoring_methods_HybridVDW_Energy_hh

// Unit Headers
#include <core/scoring/methods/HybridVDW_Energy.fwd.hh>

// Package headers
#include <core/scoring/methods/ContextIndependentTwoBodyEnergy.hh>
#include <core/scoring/AtomVDW.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>

#include <utility/vector1.hh>


// Utility headers


namespace core {
namespace scoring {
namespace methods {


class HybridVDW_Energy : public ContextIndependentTwoBodyEnergy  {
public:
	typedef ContextIndependentTwoBodyEnergy  parent;
public:

	/// @brief  C-tor
	HybridVDW_Energy();


	/// clone
	virtual
	EnergyMethodOP
	clone() const;

	/// @brief COPY C-TOR
	HybridVDW_Energy( HybridVDW_Energy const & src );


	/////////////////////////////////////////////////////////////////////////////
	// scoring
	/////////////////////////////////////////////////////////////////////////////

	virtual
	void
	setup_for_scoring( pose::Pose & pose, ScoreFunction const & ) const;

	virtual
	void
	setup_for_derivatives( pose::Pose & pose, ScoreFunction const & ) const;

	virtual
	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const &,
		EnergyMap & emap
	) const;


	virtual
	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		ScoreFunction const &,
		EnergyMap &
	) const {}


	virtual
	void
	eval_atom_derivative(
		id::AtomID const & atom_id,
		pose::Pose const & pose,
		kinematics::DomainMap const & domain_map,
		ScoreFunction const & scorefxn,
		EnergyMap const & weights,
		Vector & F1,
		Vector & F2
	) const;

	//  virtual
	//  void
	//  eval_atom_derivative(
	//   id::AtomID const & atom_id,
	//   pose::Pose const & pose,
	//   ScoreFunction const &,
	//   EnergyMap const & weights,
	//   Vector & F1,
	//   Vector & F2
	//  ) const;

	virtual
	bool
	defines_intrares_energy( EnergyMap const & /*weights*/ ) const { return false; }

	virtual
	Distance
	atomic_interaction_cutoff() const;

	virtual
	void indicate_required_context_graphs( utility::vector1< bool > & context_graphs_required ) const;

	/////////////////////////////////////////////////////////////////////////////
	// data
	/////////////////////////////////////////////////////////////////////////////

private:

	// const-ref to scoring database
	AtomVDW const & atom_vdw_;

	//std::string const atom_type_set_name_;

	//Real const vdw_scale_factor_;
	virtual
	core::Size version() const;
};


}
}
}

#endif // INCLUDED_core_scoring_ScoreFunction_HH
