// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/modeler/align/StepWisePoseAligner.cc
/// @brief
/// @details
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/stepwise/modeler/align/StepWisePoseAligner.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/rna/util.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/util.hh>
#include <core/kinematics/AtomTree.hh>
#include <core/kinematics/tree/Atom.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/pose/Pose.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/pose/full_model_info/FullModelParameters.hh>
#include <core/pose/full_model_info/util.hh>
#include <core/pose/rna/util.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/func/FlatHarmonicFunc.hh>
#include <core/scoring/constraints/CoordinateConstraint.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <utility/tools/make_vector1.hh>
#include <utility/string_util.hh>
#include <ObjexxFCL/format.hh>
#include <basic/Tracer.hh>

static THREAD_LOCAL basic::Tracer TR( "protocols.stepwise.modeler.align.StepWisePoseAligner" );
using ObjexxFCL::format::F;
using utility::tools::make_vector1;
using namespace core::scoring;
using namespace core::pose::full_model_info;

///////////////////////////////////////////////////////////////////////////////////////////
//
// Nicely factored object that (1) can figure out which atoms to use for superposition
//  of protein, RNA, or mixed poses, (2) which atoms to use for calculating RMSDs (based
//  on information stored in fixed_domain in pose full_model_info), and (3)
//  calculate those RMSDs with or without superposition.
//
// This is the central place to do RMSD calculations, and the object is used
//  by get_rmsd(), StepWiseClusterer, NativeRMSD_Screener, and setting of
//  coordinate constaints during StepWise Monte Carlo & Assembly. So if we want
//  to expand SWA/SWM to more stuff, like ligands, DNA, or metal ions, encode
//  the choices in RMSD calculations *here*.
//
// -- rhiju, 2014
//
///////////////////////////////////////////////////////////////////////////////////////////


namespace protocols {
namespace stepwise {
namespace modeler {
namespace align {

// these are 'extra' heavy atoms in the n-1 or n+1 residue that can change when residue n moves.
utility::vector1< std::string > const extra_suite_atoms_upper = make_vector1( " P  ", " OP1", " OP2", " O5'", "XO3'" /*happens in phosphate pack*/ );
utility::vector1< std::string > const extra_suite_atoms_lower = make_vector1( " O  ", "YP  ","YOP2","YOP1","YO5'" );

//Constructor
StepWisePoseAligner::StepWisePoseAligner( pose::Pose const & reference_pose ):
	reference_pose_( reference_pose ),
	rmsd_( 0.0 ),
	superimpose_rmsd_( 0.0 ),
	check_alignment_tolerance_( 1.0e-3 ),
	superimpose_over_all_instantiated_( false )
{
}

//Destructor
StepWisePoseAligner::~StepWisePoseAligner()
{}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::apply( pose::Pose & pose ){
	initialize( pose );
	do_superimposition( pose );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// calculate rmsd for pose and any 'other poses', and add up in quadrature.
Real
StepWisePoseAligner::get_rmsd_over_all_poses( pose::Pose & pose ){
	Real rmsd_over_all( 0.0 );
	Size natoms_over_all( 0 );
	superimpose_recursively( pose, rmsd_over_all, natoms_over_all );
	return rmsd_over_all;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// calculate rmsd for pose and any 'other poses', and add up in quadrature.
void
StepWisePoseAligner::superimpose_recursively( pose::Pose & pose,
	Real & rmsd_over_all, Size & natoms_over_all ){

	using namespace core::pose;
	using namespace core::pose::full_model_info;

	apply( pose ); // does superposition, fills rmsd, etc.
	Real rmsd_pose   = rmsd();
	Size natoms_pose = natoms_rmsd();
	if ( natoms_pose  == 0 ) { // happens in rna_score -- superimpose over everything, rmsd computed over nothing.
		rmsd_pose   = superimpose_rmsd();
		natoms_pose = natoms_superimpose_rmsd();
	}

	Real const total_sd = ( rmsd_over_all * rmsd_over_all * natoms_over_all) + (rmsd_pose * rmsd_pose * natoms_pose );
	natoms_over_all += natoms_pose;
	if ( natoms_over_all > 0 ) {
		rmsd_over_all = std::sqrt( total_sd / Real( natoms_over_all ) );
	} else {
		runtime_assert( std::abs( rmsd_over_all) < 1e-5 );
	}

	utility::vector1< PoseOP > const & other_pose_list = nonconst_full_model_info( pose ).other_pose_list();
	for ( Size n = 1; n <= other_pose_list.size(); n++ ) {
		superimpose_recursively( *( other_pose_list[ n ] ), rmsd_over_all, natoms_over_all );
	}
}

///////////////////////////////////////////////////////////////////////////////////
Real
StepWisePoseAligner::get_rmsd_no_superimpose( pose::Pose const & pose,
	bool const check_align /* = true */ ){
	return get_rmsd_no_superimpose( pose, *reference_pose_local_, check_align );
}

///////////////////////////////////////////////////////////////////////////////////
Real
StepWisePoseAligner::get_rmsd_no_superimpose( pose::Pose const & pose,
	pose::Pose const & reference_pose,
	bool const check_align /* = true */ ){
	superimpose_rmsd_ = 0.0;
	rmsd_ = 0.0;

	//TR << "At problem call to get_rmsd_no_superimpose, check_align is " << check_align << std::endl;
	if ( check_align ) runtime_assert( pose.annotated_sequence() == annotated_sequence_used_for_atom_id_maps_ );

	if ( superimpose_atom_id_map_.size() > 0  && check_align ) {
		superimpose_rmsd_ = rms_at_corresponding_atoms_no_super( pose, reference_pose, superimpose_atom_id_map_ );
		if ( superimpose_rmsd_ > check_alignment_tolerance_ ) {
			pose.dump_pdb( "CHECK_POSE.pdb" );
			reference_pose_local_->dump_pdb( "REFERENCE_POSE.pdb" );
			output_atom_id_map( superimpose_atom_id_map_, pose, reference_pose );
			TR << "rmsd_res_in_pose_ " << rmsd_res_in_pose_ << std::endl;
			TR << "superimpose_res_in_pose_ " << superimpose_res_in_pose_ << std::endl;
			TR << "superimpose rmsd: " << superimpose_rmsd_ << "  is greater than " << check_alignment_tolerance_ << std::endl;
		}
		runtime_assert( superimpose_rmsd_ <= check_alignment_tolerance_ );
	}

	if ( calc_rms_atom_id_map_.size() > 0 ) {
		rmsd_ = rms_at_corresponding_atoms_no_super( pose, reference_pose, calc_rms_atom_id_map_ );
	}

	return rmsd_;
}

///////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::initialize( pose::Pose const & pose ){
	update_reference_pose_local( pose );
	get_rmsd_res_and_superimpose_res_in_pose( pose );
	update_superimpose_atom_id_map( pose );
	update_calc_rms_atom_id_map( pose );
	annotated_sequence_used_for_atom_id_maps_ = pose.annotated_sequence(); // used for a consistency check.
}

///////////////////////////////////////////////////////////////////////////////////
bool
match_up_to_rna_dna( char const & nt1, char const & nt2 ) {
	if ( nt1 == nt2 ) return true;
	if ( nt1 == 't' && nt2 == 'u' ) return true;
	if ( nt1 == 'u' && nt2 == 't' ) return true;
	return false;
}

///////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::update_reference_pose_local( pose::Pose const & pose ){

	utility::vector1< Size > const & res_list = get_res_list_from_full_model_info_const( pose );
	utility::vector1< Size > const res_list_in_reference = get_res_list_in_reference( pose );
	std::string const & full_sequence = const_full_model_info( pose ).full_sequence();

	std::string const full_sequence_stripped = core::pose::rna::remove_bracketed( full_sequence );

	// local working copy, mutated in cases where nucleotides have been designed ('n')
	if ( reference_pose_local_ == 0 ) reference_pose_local_ = reference_pose_.clone();

	//TR << "     Sequence " << pose.sequence() << std::endl;
	//TR << "full Sequence " << full_sequence   << std::endl;

	for ( Size n = 1; n <= pose.total_residue(); n++ ) {
		char const pose_nt = pose.sequence()[ n-1 ];
		if ( res_list_in_reference[n] == 0 ) continue;

		//TR << "Evaluating residue " << n << pose.residue_type(n).name1() << std::endl;
		//TR << "Compare to " << n - 1 + offset << " due to offset " << offset << ": " << full_sequence[ n - 1 + offset ] << std::endl;

		if ( full_sequence_stripped[ res_list[ n ] - 1 ] == 'n' ) {
			if ( reference_pose_local_->sequence()[ res_list_in_reference[n] - 1 ] != pose_nt ) {
				// need to generalize to protein... should be trivial.
				pose::rna::mutate_position( *reference_pose_local_, res_list_in_reference[n], pose_nt );
			}
		} else {
			runtime_assert( full_sequence_stripped[ res_list[ n ] - 1 ] == pose_nt );
		}
		runtime_assert( match_up_to_rna_dna( reference_pose_local_->sequence()[ res_list_in_reference[n] - 1], pose_nt ) );
	}
}


///////////////////////////////////////////////////////////////////////////////////
// where each residue in pose ends up in the reference_pose
//  a little cumbersome -- actually convert to full_model and then to
//  conventional number/chain, and then back.
//
// Have to do this since we no longer guarantee that reference_pose has
//  same full_model numbering (e.g., from a fasta file) as working pose --
//  instead need to match up number & chain.
//
// Could also use PDBInfo for this.
//
utility::vector1< Size >
StepWisePoseAligner::get_res_list_in_reference( pose::Pose const & pose ) const {

	utility::vector1< Size > const & res_list = get_res_list_from_full_model_info_const( pose );
	utility::vector1< Size > const & reference_pose_res_list = get_res_list_const( reference_pose_ );
	FullModelParameters const & full_model_parameters =  *(const_full_model_info( pose ).full_model_parameters());
	FullModelParameters const & reference_full_model_parameters =  *(const_full_model_info( reference_pose_ ).full_model_parameters());

	utility::vector1< Size > res_list_in_reference;
	for ( Size n = 1; n <= pose.total_residue(); n++ ) {

		bool found_it( false );
		std::pair< int, char > const resnum_and_chain = full_model_parameters.full_to_conventional_resnum_and_chain( res_list[ n ] );
		Size const & resnum( resnum_and_chain.first );
		Size const & chain( resnum_and_chain.second );
		if ( reference_full_model_parameters.has_conventional_residue( resnum, chain ) ) {
			Size const reference_resnum = reference_full_model_parameters.conventional_to_full( resnum, chain );
			if ( reference_pose_res_list.has_value( reference_resnum ) ) {
				found_it = true;
				res_list_in_reference.push_back( reference_pose_res_list.index( reference_resnum ) );
			}
		}

		if ( !found_it ) res_list_in_reference.push_back( 0 );
	}
	return res_list_in_reference;
}


///////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::update_calc_rms_atom_id_map( pose::Pose const & pose ){

	// first need to slice up reference_pose to match residues in actual pose.
	// define atoms over which to compute RMSD, using rmsd_res.
	FullModelInfo const & full_model_info = const_full_model_info( pose );
	utility::vector1< Size > const & res_list = full_model_info.res_list();
	if ( res_list.size() == 0 ) return; // special case -- blank pose.

	utility::vector1< Size > calc_rms_res;
	for ( Size i = 1; i <= rmsd_res_in_pose_.size(); i++ ) {
		Size const & n = rmsd_res_in_pose_[ i ];
		if ( user_defined_calc_rms_res_.size() > 0 && !user_defined_calc_rms_res_.has_value( n ) ) continue;
		calc_rms_res.push_back( n );
	}

	// super special case -- is this still a possibility?
	if ( calc_rms_res.size() == 0 && pose.total_residue() == 1 ) calc_rms_res.push_back( 1 );

	get_calc_rms_atom_id_map( calc_rms_atom_id_map_, pose, calc_rms_res );
}

///////////////////////////////////////////////////////
void
StepWisePoseAligner::get_calc_rms_atom_id_map( std::map< id::AtomID, id::AtomID > & calc_rms_atom_id_map,
	core::pose::Pose const & pose,
	utility::vector1 < Size > const & calc_rms_res ) const {

	using namespace core::chemical;

	FullModelInfo const & full_model_info = const_full_model_info( pose );
	utility::vector1< Size > const & res_list = full_model_info.res_list();
	utility::vector1< Size > const res_list_in_reference = get_res_list_in_reference( pose );
	utility::vector1< Size > const & fixed_domain_map = full_model_info.fixed_domain_map();

	calc_rms_atom_id_map.clear();
	for ( Size k = 1; k <= calc_rms_res.size(); k++ ) {
		Size const n = calc_rms_res[ k ];
		for ( Size q = 1; q <= pose.residue_type( n ).nheavyatoms(); q++ ) {
			add_to_atom_id_map_after_checks( calc_rms_atom_id_map,
				pose.residue_type( n ).atom_name( q ),
				n, res_list_in_reference[ n ],
				pose, *reference_pose_local_ );
		}
	}

	// additional RNA & protein 'suites' (connections from i to i+1) over which to calculate RMSD
	utility::vector1< Size > calc_rms_suites;
	for ( Size n = 1; n < pose.total_residue(); n++ ) {

		if ( ( pose.residue_type( n ).is_RNA()     && pose.residue_type( n + 1 ).is_RNA() ) ||
				( pose.residue_type( n ).is_protein() && pose.residue_type( n + 1 ).is_protein() ) ) {
			// Atoms at ends of rebuilt loops:
			if ( !pose.fold_tree().is_cutpoint( n ) || pose.residue_type( n ).has_variant_type( CUTPOINT_LOWER ) ) {
				if ( calc_rms_res.has_value( n ) || calc_rms_res.has_value( n+1) ) {
					calc_rms_suites.push_back( n ); continue;
				}
			}
			// Domain boundaries:
			if ( (res_list[ n+1 ] == res_list[ n ] + 1) &&
					fixed_domain_map[ res_list[ n ] ] != 0 &&
					fixed_domain_map[ res_list[ n+1 ] ] != 0 &&
					fixed_domain_map[ res_list[ n ] ] != fixed_domain_map[ res_list[ n+1 ] ] ) {
				calc_rms_suites.push_back( n );
			}
		}
	}

	for ( Size k = 1; k <= calc_rms_suites.size(); k++ ) {
		Size const n = calc_rms_suites[ k ];
		for ( Size q = 1; q <= extra_suite_atoms_upper.size(); q++ ) {
			add_to_atom_id_map_after_checks( calc_rms_atom_id_map, extra_suite_atoms_upper[ q ],
				n+1, res_list_in_reference[ n+1 ],
				pose, *reference_pose_local_ );
		}
		for ( Size q = 1; q <= extra_suite_atoms_lower.size(); q++ ) {
			add_to_atom_id_map_after_checks( calc_rms_atom_id_map, extra_suite_atoms_lower[ q ],
				n, res_list_in_reference[ n ],
				pose, *reference_pose_local_ );
		}
	}
	//  output_atom_id_map( calc_rms_atom_id_map, pose, *reference_pose_local_ );
}


///////////////////////////////////////////////////////////////////////////////////
// define superposition atoms. Should be over atoms in any fixed domains. This should be
// the 'inverse' of complete_moving_atoms (of which calc_rms atoms is a subset).
void
StepWisePoseAligner::update_superimpose_atom_id_map( pose::Pose const & pose ) {
	using namespace core::id;
	utility::vector1< Size > const res_list_in_reference = get_res_list_in_reference( pose );

	// everything that can move.
	get_calc_rms_atom_id_map( complete_moving_atom_id_map_, pose, rmsd_res_in_pose_ );
	superimpose_atom_id_map_.clear();
	for ( Size n = 1; n <= pose.total_residue(); n++ ) {
		if ( !superimpose_res_in_pose_.has_value( n ) ) continue;
		bool const sample_sugar = check_sample_sugar_in_full_model_info( pose, n );
		for ( Size q = 1; q <= pose.residue_type( n ).nheavyatoms(); q++ ) {
			if ( complete_moving_atom_id_map_.find( AtomID( q, n ) ) != complete_moving_atom_id_map_.end() ) continue;
			std::string const atom_name = pose.residue_type( n ).atom_name( q );
			if ( extra_suite_atoms_upper.has_value( atom_name ) ) continue; // never use phosphates to superimpose
			if ( extra_suite_atoms_lower.has_value( atom_name ) ) continue; // never use carbonyl oxygens to superimpose
			if ( sample_sugar && core::chemical::rna::sugar_atoms.has_value( atom_name ) ) continue;
			add_to_atom_id_map_after_checks( superimpose_atom_id_map_,
				atom_name,
				n, res_list_in_reference[n],
				pose, *reference_pose_local_ );
		}
	}

	if ( superimpose_over_all_instantiated_ ) superimpose_atom_id_map_ = complete_moving_atom_id_map_;

	// What if there weren't any fixed atoms?
	// How about superposition over just N-CA-C triad (protein) or nucleobase (RNA), which should not change.
	if ( superimpose_atom_id_map_.size() < 3 ) {
		superimpose_atom_id_map_ = get_root_triad_atom_id_map( pose );
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////
Real
StepWisePoseAligner::do_superimposition( pose::Pose & pose ) {

	superimpose_rmsd_ = 0.0;
	rmsd_ = 0.0;

	if ( superimpose_atom_id_map_.size() > 0 ) {
		runtime_assert( superimpose_atom_id_map_.size() >= 3 );
		superimpose_rmsd_ = scoring::superimpose_pose( pose, *reference_pose_local_, superimpose_atom_id_map_ );
		if ( calc_rms_atom_id_map_.size() > 0 ) {
			rmsd_ = rms_at_corresponding_atoms_no_super( pose, *reference_pose_local_, calc_rms_atom_id_map_ );
		}
	}


	TR << "RMSD " << F(5,3,rmsd_) <<
		" (" << natoms_rmsd() << " atoms in " << make_tag_with_dashes( sub_to_full(rmsd_res_in_pose_,pose) ) << "), superimposed on " << superimpose_atom_id_map_.size() << " atoms in " <<
		make_tag_with_dashes( sub_to_full(superimpose_res_in_pose_,pose) ) << " (RMSD " <<
		F(9,7,superimpose_rmsd_) << ") " << std::endl;

	return rmsd_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Are there any fixed domains in the pose? If so, superimpose on the first on of those, and calculate rmsd over
// everything else.
// If no fixed domains, calculate rmsd over everything.
Size
StepWisePoseAligner::get_rmsd_res_and_superimpose_res_in_pose( pose::Pose const & pose ) {

	utility::vector1< Size > domain_map = get_fixed_domain_from_full_model_info_const( pose );

	if ( user_defined_calc_rms_res_.size() > 0 ) {
		for ( Size n = 1; n <= user_defined_calc_rms_res_.size(); n++ )  domain_map[ user_defined_calc_rms_res_[n] ] = 0;
	}

	// First choice for primary domain is the domain encompassing the root. That
	// should stay fixed.
	Size d_primary = domain_map[ pose.fold_tree().root() ];

	if ( d_primary > 0 ) {
		if ( root_partition_res_.size() > 0 ) runtime_assert( root_partition_res_.has_value( pose.fold_tree().root() ) );
	} else {
		// Next strategy, figure out 'primary' domain number. Smallest number that is not zero.
		// must be drawn from root_partition (if that partition is defined)
		for ( Size n = 1; n <= pose.total_residue(); n++ ) {
			if ( root_partition_res_.size() > 0 && !root_partition_res_.has_value( n ) ) continue;
			Size const d = domain_map[ n ];
			if ( d > 0 && ( d_primary == 0 || d < d_primary ) ) d_primary = d;
		}
	}

	rmsd_res_in_pose_.clear();
	superimpose_res_in_pose_.clear();
	if ( d_primary == 0 ) { // superimpose on everything.
		for ( Size n = 1; n <= pose.total_residue(); n++ ) {
			rmsd_res_in_pose_.push_back( n );
			if ( root_partition_res_.size() == 0 || root_partition_res_.has_value( n ) ) superimpose_res_in_pose_.push_back( n );
		}
	} else { // superimpose on primary domain, calculate rmsd over rest.
		for ( Size n = 1; n <= pose.total_residue(); n++ ) {
			if ( superimpose_over_all_instantiated_ ) { // for final RMSDs, superimpose over everything, calc RMS over everything (?)
				superimpose_res_in_pose_.push_back( n );
				rmsd_res_in_pose_.push_back( n );
			} else {
				if ( domain_map[ n ] == d_primary ) {
					if ( root_partition_res_.size() == 0 || root_partition_res_.has_value( n ) ) superimpose_res_in_pose_.push_back( n );
				} else {
					if ( !root_partition_res_.has_value( n ) )  rmsd_res_in_pose_.push_back( n );
				}
			}
		}
	}

	return d_primary;
}

/////////////////////////////////////////////////////////////////////////
// adapted from arvind kannan's original hack -- rhiju, 2014
void
StepWisePoseAligner::add_coordinate_constraints_from_map( pose::Pose & pose, pose::Pose const & reference_pose,
	std::map< id::AtomID, id::AtomID > const & atom_id_map,
	core::Real const & constraint_x0, core::Real const & constraint_tol ) const {
	using namespace core::scoring::func;
	using namespace core::scoring::constraints;

	Size const my_anchor( pose.fold_tree().root() ); //Change to use the root of the current foldtree as done by Rocco in AtomCoordinateCstMover - JAB.

	ConstraintSetOP cst_set = pose.constraint_set()->clone();
	FuncOP constraint_func( new FlatHarmonicFunc( constraint_x0, 1.0, constraint_tol ) );
	//FuncOP constraint_func = new FadeFunc( -0.7, 1.5, 0.8, -1.0, 0.0)) );

	for ( std::map< id::AtomID, id::AtomID >::const_iterator
			it=atom_id_map.begin(), it_end = atom_id_map.end(); it != it_end; ++it ) {
		id::AtomID const mapped_atom = it->second;
		ConstraintOP constraint( new CoordinateConstraint ( it->first, id::AtomID(1, my_anchor),
			reference_pose.residue(mapped_atom.rsd()).xyz(mapped_atom.atomno()),
			constraint_func ) );
		cst_set->add_constraint( constraint );
	}

	pose.constraint_set( cst_set );

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::create_coordinate_constraints( pose::Pose & pose,
	Real const rmsd_screen ){

	using namespace core::pose::full_model_info;

	if ( rmsd_screen == 0.0 ) return;
	runtime_assert( reference_pose_local_ != 0 ); // needs to be setup by apply() above.

	utility::vector1< Size > const res_list_in_reference = get_res_list_in_reference( pose );
	std::map< id::AtomID, id::AtomID> coordinate_constraint_atom_id_map;
	for ( Size n = 1; n <= pose.total_residue(); n++ ) {
		for ( Size q = 1; q <= pose.residue_type( n ).nheavyatoms(); q++ ) {
			add_to_atom_id_map_after_checks( coordinate_constraint_atom_id_map,
				pose.residue_type( n ).atom_name( q ),
				n, res_list_in_reference[ n ],
				pose, *reference_pose_local_ );
		}
	}

	Real const constraint_x0  = 0.0; // stay near native.
	Real const constraint_tol = rmsd_screen; // no penalty for deviations up to this amount. After that, (x - tol)^2.
	add_coordinate_constraints_from_map( pose, *reference_pose_local_, coordinate_constraint_atom_id_map,
		constraint_x0, constraint_tol );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// encodes different choices between RNA and proteins:
//     all-heavy-atom for RNA -- no terminal phosphates.
//     just backbone-atoms for proteins.
bool
StepWisePoseAligner::do_checks( std::string const & atom_name, Size const & n, pose::Pose const & pose ) const {

	if ( ! pose.residue_type( n ).has( atom_name ) ) return false;
	Size const idx = pose.residue_type( n ).atom_index( atom_name );
	if ( pose.residue_type( n ).is_virtual( idx ) ) return false;

	if ( pose.residue_type( n ).is_protein() ) {
		if ( idx >= pose.residue_type( n ).first_sidechain_atom() && idx <= pose.residue_type( n ).nheavyatoms() ) return false;
		if ( idx >  pose.residue_type( n ).first_sidechain_hydrogen()  && idx <= pose.residue_type( n ).natoms() ) return false;
	}

	// no terminal phosphates... (or more generally connection atoms)
	if ( extra_suite_atoms_upper.has_value( atom_name ) &&
			!pose.residue_type( n ).has_variant_type( "CUTPOINT_UPPER" ) &&
			( n == 1 || pose.fold_tree().is_cutpoint( n - 1  ) ) ) return false;

	if ( extra_suite_atoms_lower.has_value( atom_name ) &&
			!pose.residue_type( n ).has_variant_type( "CUTPOINT_LOWER" ) &&
			( n == pose.total_residue() || pose.fold_tree().is_cutpoint( n ) ) ) return false;

	// Do not align over the fourth chi atom. This is either a hydroxyl H (i.e.
	// fails the no heavy atom check, basically not a "backbone atom") or it is
	// a methyl in a noncanonical (heavy, but not gonna stay too fixed ideally)
	//if ( pose.residue_type( n ).is_RNA() &&
	//  atom_name == pose.residue_type( n ).atom_name( pose.residue_type( n ).chi_atoms( 4 )[ 4 ] ) ) return false;

	return true;
}

bool
StepWisePoseAligner::add_to_atom_id_map_after_checks( std::map< id::AtomID, id::AtomID> & atom_id_map,
	std::string const & atom_name,
	Size const & n1, Size const & n2,
	pose::Pose const & pose1, pose::Pose const & pose2,
	bool const do_the_checks /* = true */ ) const {

	using namespace core::id;

	if ( n1 == 0 || n2 == 0 ) return false;
	runtime_assert ( n1 >= 1 && n1 <= pose1.total_residue() );
	runtime_assert ( n2 >= 1 && n2 <= pose2.total_residue() );
	if ( pose1.residue_type( n1 ).aa() != pose2.residue_type( n2 ).aa() &&
			!rna_dna_match( pose1.residue_type( n1 ).aa(), pose2.residue_type( n2 ).aa() ) ) {
		TR << "pose1 at n1 " << n1 << " has aa: " << pose1.residue_type( n1 ).aa() << "; vs pose2 at n2 " << n2 << " has aa: " <<  pose2.residue_type( n2 ).aa()  << std::endl;
		runtime_assert( pose1.residue_type( n1 ).aa() == pose2.residue_type( n2 ).aa() );
	}

	if ( do_the_checks ) {
		if ( !do_checks( atom_name, n1, pose1 ) ) return false;
		if ( !do_checks( atom_name, n2, pose2 ) ) return false;
	}

	Size const idx1 = pose1.residue_type( n1 ).atom_index( atom_name );
	Size const idx2 = pose2.residue_type( n2 ).atom_index( atom_name );
	atom_id_map[ AtomID( idx1, n1 ) ] = AtomID( idx2, n2 );
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::output_atom_id_map( std::map< id::AtomID, id::AtomID > const & atom_id_map ) const {
	for ( std::map < id::AtomID, id::AtomID >::const_iterator it = atom_id_map.begin(),
			end = atom_id_map.end(); it != end; ++it ) {
		TR << it->first << " mapped to " << it->second << std::endl;
	}
	TR << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
StepWisePoseAligner::output_atom_id_map( std::map< id::AtomID, id::AtomID > const & atom_id_map,
	pose::Pose const & pose1,
	pose::Pose const & pose2 ) const {
	for ( std::map < id::AtomID, id::AtomID >::const_iterator it = atom_id_map.begin(),
			end = atom_id_map.end(); it != end; ++it ) {
		TR << it->first << " " << pose1.residue_type( it->first.rsd() ).atom_name( it->first.atomno() ) <<
			" mapped to " <<
			it->second << " " << pose2.residue_type( it->second.rsd() ).atom_name( it->second.atomno() ) << "  distance: " << ( pose1.xyz( it->first ) - pose2.xyz( it->second) ).length() << std::endl;
	}
	TR << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::map< id::AtomID, id::AtomID >
StepWisePoseAligner::get_root_triad_atom_id_map( pose::Pose const & pose ) const {
	Size const root_res = pose.fold_tree().root();
	Size const root_atomno = get_root_residue_root_atomno( pose.residue( root_res ), pose.fold_tree() );
	core::kinematics::tree::AtomCOP root_atom ( pose.atom_tree().atom_dont_do_update( id::AtomID( root_atomno, root_res ) ).get_self_ptr() );
	utility::vector1< core::kinematics::tree::AtomCOP > stub_atoms =
		make_vector1( root_atom->stub_atom1(),
		root_atom->stub_atom2(),
		root_atom->stub_atom3() );

	std::map< id::AtomID, id::AtomID > root_triad_atom_id_map;
	//utility::vector1< Size > const & res_list = get_res_list_from_full_model_info_const( pose );
	utility::vector1< Size > const res_list_in_reference = get_res_list_in_reference( pose );
	for ( Size i = 1; i <= 3; i++ ) {
		Size const n = stub_atoms[i]->id().rsd();
		Size const q = stub_atoms[i]->id().atomno();
		add_to_atom_id_map_after_checks( root_triad_atom_id_map,
			pose.residue_type( n ).atom_name( q ),
			n, res_list_in_reference[ n ],
			pose, *reference_pose_local_, false /* do_the_checks */ );
	}
	return root_triad_atom_id_map;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool
StepWisePoseAligner::check_matching_atom_names( pose::Pose const & pose1, pose::Pose const & pose2, bool const verbose /* = true */ ){
	return check_matching_atom_names( pose1, pose2, calc_rms_atom_id_map_, verbose );
}
///////////////////////////////////////////////////////////////////////////////////////////////////
bool
StepWisePoseAligner::check_matching_atom_names( pose::Pose const & pose1, pose::Pose const & pose2,
	std::map< id::AtomID, id::AtomID > const & atom_id_map,
	bool const verbose /* = true */ ){
	for ( std::map< id::AtomID, id::AtomID >::const_iterator
			it=atom_id_map.begin(), it_end = atom_id_map.end(); it != it_end; ++it ) {
		id::AtomID const & atom_id1 = it->first;
		id::AtomID const & atom_id2 = it->second;
		std::string const & atom_name1 = pose1.residue_type( atom_id1.rsd() ).atom_name( atom_id1.atomno() );
		std::string const & atom_name2 = pose2.residue_type( atom_id2.rsd() ).atom_name( atom_id2.atomno() );
		if ( atom_name1 != atom_name2  ) {
			if ( verbose ) TR << "Mismatch! " << atom_name1 << " [" << atom_id1 << "]  in first pose does not match  " << atom_name2 << " [" << atom_id2 << "] in second pose" << std::endl;
			return false;
		}
	}
	return true;
}

} //align
} //modeler
} //stepwise
} //protocols
