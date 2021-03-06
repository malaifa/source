// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/constraints/ResidueConstraint.hh
///
/// @brief implements favor_native_residue constraints, assigning an energy bonus
/// to a residue if it matches the identity within the constraint object
/// @author Sarel Fleishman


#ifndef INCLUDED_core_scoring_constraints_ResidueTypeConstraint_hh
#define INCLUDED_core_scoring_constraints_ResidueTypeConstraint_hh

#include <core/scoring/constraints/ResidueTypeConstraint.fwd.hh>
#include <core/scoring/constraints/Constraint.hh>

#include <core/conformation/Conformation.fwd.hh>
#include <core/id/AtomID.hh>


#include <utility/vector1.hh>

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION


namespace core {
namespace scoring {
namespace constraints {

/// @brief This class favors a particular residue identity at a particular position by reducing its res_type energy.
class ResidueTypeConstraint : public Constraint
{
public:
	typedef core::Real Real;
public:
	ResidueTypeConstraint();

	ResidueTypeConstraint(
		pose::Pose const & pose,
		Size seqpos,
		Real native_residue_bonus
	);


	ResidueTypeConstraint(
		pose::Pose const & pose,
		Size seqpos,
		std::string AAname,
		Real favor_native_bonus
	);

	ResidueTypeConstraint(
		Size seqpos,
		std::string aa_in,
		std::string name3_in,
		core::Real bonus_in
	);

	virtual ~ResidueTypeConstraint();

	virtual
	Size
	natoms() const { return 0; }

	virtual
	AtomID const &
	atom( Size const ) const {
		utility_exit_with_message("ResidueTypeConstraint is not atom-based!.");
		return core::id::BOGUS_ATOM_ID;  // required for compilation on Windows
	}

	virtual
	utility::vector1< core::Size >
	residues() const;

	void
	show( std::ostream & out ) const;

	virtual
	ConstraintOP
	remap_resid( core::id::SequenceMapping const &seqmap ) const;

	virtual bool operator == ( Constraint const & other ) const;
	virtual bool same_type_as_me( Constraint const & other ) const;

	virtual ConstraintOP remapped_clone(
		pose::Pose const & src,
		pose::Pose const & dest,
		id::SequenceMappingCOP map = NULL
	) const;

	virtual
	void
	score( func::XYZ_Func const & xyz_func, EnergyMap const & weights, EnergyMap & emap ) const;

	virtual
	void
	fill_f1_f2(
		AtomID const & atom,
		func::XYZ_Func const & xyz,
		Vector & F1,
		Vector & F2,
		EnergyMap const & weights
	) const;

	virtual ConstraintOP
	clone() const;
private:
	Size seqpos_;
	std::string AAname;
	std::string rsd_type_name3_;
	core::Real favor_native_bonus_;
#ifdef    SERIALIZATION
public:
	template< class Archive > void save( Archive & arc ) const;
	template< class Archive > void load( Archive & arc );
#endif // SERIALIZATION

}; // RotamerConstraint


} // namespace constraints
} // namespace scoring
} // namespace core

#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( core_scoring_constraints_ResidueTypeConstraint )
#endif // SERIALIZATION


#endif // INCLUDED_core_scoring_constraints_ResidueTypeConstraint_HH
