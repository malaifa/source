// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.
/// @file   FlexPepDockingProtocol.cc
///
/// @brief protocol for docking flexible peptides onto globular proteins
/// @date August 5, 2008
/// @author Barak Raveh / Nir London
/// @author Modified by Nawsad Alam

// Project headers
#include <core/conformation/ResidueFactory.hh>
#include <core/chemical/AA.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/fragment/FragSet.hh>
#include <core/fragment/ConstantLengthFragSet.hh>

#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/conformation/Conformation.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pack/rotamer_set/UnboundRotamersOperation.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/NoRepackDisulfides.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/constraints/Constraint.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/constraints/CoordinateConstraint.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/Constraints.hh>
#include <core/scoring/func/HarmonicFunc.hh>
#include <core/scoring/constraints/util.hh>
#include <core/scoring/Energies.hh>
#include <core/types.hh>
#include <core/scoring/constraints/SiteConstraint.hh>

// Unit Headers
#include <protocols/flexpep_docking/FlexPepDockingFlags.hh>
#include <protocols/flexpep_docking/FlexPepDockingLowRes.hh>
#include <protocols/flexpep_docking/FlexPepDockingAbInitio.hh>
#include <protocols/flexpep_docking/FlexPepDockingPoseMetrics.hh>
#include <protocols/flexpep_docking/FlexPepDockingProtocol.hh>
#include <protocols/flexpep_docking/FlexPepDockingProtocolCreator.hh>
#include <protocols/scoring/Interface.hh>
#include <protocols/toolbox/task_operations/RestrictToInterface.hh>
#include <protocols/filters/Filter.fwd.hh>
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/Job.hh>
#include <protocols/simple_moves/BackboneMover.hh>
#include <protocols/backrub/BackrubMover.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/simple_moves/MinMover.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh>
#include <protocols/simple_moves/RotamerTrialsMover.hh>
#include <protocols/simple_moves/RotamerTrialsMinMover.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/comparative_modeling/LoopRelaxMover.hh>
#include <protocols/viewer/viewers.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <protocols/rigid/RB_geometry.hh>

// Basic Headers
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/docking.OptionKeys.gen.hh>
#include <basic/options/keys/loops.OptionKeys.gen.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>
#include <basic/prof.hh>
#include <basic/Tracer.hh>
#include <basic/datacache/DataMap.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>

// Numeric Headers
#include <numeric/random/random.hh>
#include <numeric/angle.functions.hh>
#include <numeric/PCA.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyzVector.io.hh>

// Utility Headers
#include <utility/file/FileName.hh>
#include <utility/tag/Tag.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/keys/Key3Vector.hh>

// ObjexxFCL Headers
#include <ObjexxFCL/FArray1D.hh>

// C++ headers
#include <string>
#include <set>
#include <sstream>
#include <cstdio>
#include <algorithm>

using basic::T;
using basic::Error;
using basic::Warning;

static THREAD_LOCAL basic::Tracer TR( "FlexPepDockingProtocol" );
using namespace core;
using namespace ObjexxFCL;

namespace protocols {
namespace flexpep_docking {

// constructor
FlexPepDockingProtocol::FlexPepDockingProtocol(Size const rb_jump_in)
: Mover(),
	rb_jump_(rb_jump_in),
	flags_(),
	//create a metrics object
	fpdock_metrics_(flags_.clone())
{
	Mover::type( "FlexPepDockingProtocol" );
	set_default();
}

// constructor
FlexPepDockingProtocol::FlexPepDockingProtocol(
	Size const rb_jump_in,
	bool const fullatom,
	bool const view
) :
	Mover(),
	rb_jump_(rb_jump_in),
	flags_(),
	fpdock_metrics_(flags_.clone())
{
	Mover::type( "FlexPepDockingProtocol" );
	set_default();
	fullatom_ = fullatom;
	view_ = view;
}

// empty destructor - for good includiong of OP clasesses
FlexPepDockingProtocol::~FlexPepDockingProtocol()
{
}

// cloner
protocols::moves::MoverOP FlexPepDockingProtocol::clone() const
{
	FlexPepDockingProtocolOP clonedObj( new FlexPepDockingProtocol( rb_jump_, fullatom_, view_ ) );
	clonedObj->scorefxn_ = scorefxn_->clone();
	clonedObj->flags_ = flags_;
	return clonedObj;
}

// @brief setup that is called from constructor
void FlexPepDockingProtocol::set_default()
{
	using namespace basic::options;
	using namespace core::scoring;
	using namespace core::pack;
	using namespace core::pack::task;

	fullatom_ = option[ OptionKeys::out::file::fullatom ](); // TODO: use flags_
	view_ = option[ OptionKeys::docking::view ](); // TODO: use flags_, and don't use docking:: options
	scorefxn_ = core::scoring::get_score_function(); // from cmd-line (-score:weights -score:patch)
	scorefxn_lowres_ = ScoreFunctionFactory::create_score_function ( CENTROID_WTS, "score4L") ; // centroid score in between chains

	//reset interface measures:
	if_metrics_["I_sc"] = 0.0;
	if_metrics_["I_bsa"] = 0.0;
	if_metrics_["I_hb"] = 0.0;
	if_metrics_["I_pack"] = 0.0;
	if_metrics_["I_unsat"] = 0.0;

	// prepare unbound rotamer task-operation (reading from cmd-line -unboundrot)
	rotamer_set::UnboundRotamersOperationOP unboundrot_oper( new rotamer_set::UnboundRotamersOperation() );
	unboundrot_oper->initialize_from_command_line();
	operation::AppendRotamerSetOP append_ubrot_taskoper( new operation::AppendRotamerSet( unboundrot_oper ) );

	// all-protein packer settings:
	using core::pack::task::operation::TaskOperationCOP;
	allprotein_tf_ = core::pack::task::TaskFactoryOP( new task::TaskFactory );
	allprotein_tf_->push_back( TaskOperationCOP( new operation::InitializeFromCommandline ) ); // -ex1,ex2,use_input_sc,etc.
	allprotein_tf_->push_back( TaskOperationCOP( new operation::IncludeCurrent ) ); // TODO: since our input is a prepacked structure, I always included its side-chains (these are NOT necessarily the native side-chains). But, maybe this should be left to the user (Barak)
	//allprotein_tf_->push_back( TaskOperationCOP( new operation::RestrictToRepacking ) ); // prevents design
	allprotein_tf_->push_back( append_ubrot_taskoper ); // add support to -unboundrot
	if ( option[OptionKeys::packing::resfile].user() ) {
		allprotein_tf_->push_back( TaskOperationCOP( new operation::ReadResfile ) );
	}

	/*core::pack::task::operation::RestrictResidueToRepackingOP receptor_protector_oper_;
	receptor_protector_oper_ = core::pack::task::operation::RestrictResidueToRepackingOP( new operation::RestrictResidueToRepacking() );
	receptor_protector_oper_->clear();
	for ( int i = flags_.receptor_first_res(); i <= receptor_last_res(); ++i ){
	receptor_protector_oper_->include_residue(i);
	}*/

	if ( ! flags_.design_peptide ) {
		allprotein_tf_->push_back( core::pack::task::operation::TaskOperationCOP( new core::pack::task::operation::RestrictToRepacking ) );
	}

	/*if ( flags_.design_peptide )
	allprotein_tf_->push_back( receptor_protector_oper_ );*/

	// interface packer settings:
	interface_tf_ = core::pack::task::TaskFactoryOP( new task::TaskFactory(*allprotein_tf_) ); // based on settings for allprotein_tf_
	if ( ! flags_.pep_fold_only ) {
		interface_tf_->push_back( TaskOperationCOP( new protocols::toolbox::task_operations::RestrictToInterface( rb_jump_ ) ));
	}

	interface_packer_ = protocols::simple_moves::PackRotamersMoverOP( new protocols::simple_moves::PackRotamersMover() );
	interface_packer_->score_function( scorefxn_ );
	interface_packer_->task_factory( interface_tf_ );

	movemap_ = core::kinematics::MoveMapOP( new core::kinematics::MoveMap() );
	movemap_minimizer_ = core::kinematics::MoveMapOP( new core::kinematics::MoveMap() );


	//design packer setting
	/*design_tf_ = core::pack::task::TaskFactoryOP( new tast::TaskFactory );
	design_tf_->push_back( TaskOperationCOP( new operation::InitializeFromCommandline ) );
	design_tf_->push_back( TaskOperationCOP( new operation::InitializeFromCommandline ) );
	design_tf_->push_back( TaskOperationCOP( new operation::IncludeCurrent ) );
	design_tf_->push_back( TaskOperationCOP( append_ubrot_taskoper );
	design_tf_->push_back( TaskOperationCOP( new operation::ReadResfile );
	design_tf_->push_back( TaskOperationCOP( new protocols::toolbox::task_operations::RestrictToInterface( rb_jump_ ) ));

	receptor_protector_oper_ = options::RestrictResidueToRepackingOP( new options::restrict_to_repacking() );
	receptor_protector_oper_->clear();
	for ( int i = flags_.receptor_first_res(); i <= receptor_last_res(); ++i ){
	receptor_protector_oper_->include_residue(i);
	}

	if ( flags_.prevent_receptor_design )
	design_tf_->push_back( receptor_protector_oper_ );

	design_packer_ = protocols::simple_moves::PackRotamersMoverOP( new protocols::simple_moves::PackRotamersMover() );
	design_packer_->score_function( scorefxn_ );
	design_packer_->task_factory( design_tf_ );
	*/


	// Loop modeling options
	// NOTE: most LoopRelax options are initiated automatically from cmd-line
	// TODO: should refine be set to default? we want to guarantee a full-atom output
	loop_relax_mover_ = protocols::comparative_modeling::LoopRelaxMoverOP( new protocols::comparative_modeling::LoopRelaxMover() );
	loop_relax_mover_->fa_scorefxn(scorefxn_); // TODO: for now we do not touch the centroid part...
	loop_relax_mover_->remodel("no"); // remodel only in centroid part, if at all? // TODO: rethink this
	loop_relax_mover_->relax("no"); // We don't want structure relaxation by all means // TODO: inform user
	// we must have a full-atom output here // TODO: inform user about this
	if ( loop_relax_mover_->refine() == "no" ) {
		loop_relax_mover_->refine("refine_kic");
	}
	if ( option[ OptionKeys::loops::frag_files ].user() ) {
		// these protocols optionally take a fragment set .. only load if
		// specified
		utility::vector1< core::fragment::FragSetOP > frag_libs;
		protocols::loops::read_loop_fragments( frag_libs );
		loop_relax_mover_->frag_libs( frag_libs );
	}
}


///////////////////////////////////////////
/// @brief setup the peptide docking fold tree
// jumps are created from the receptor, and then
// between consecutive peptide anchors:
///////////////////////////////////////////
void
FlexPepDockingProtocol::setup_foldtree( pose::Pose & pose )
{
	using namespace std;

	// This assert does not hold if we add ligand residues other than the protein and the peptide
	// runtime_assert( flags_.receptor_nres() + flags_.peptide_nres() == (int)pose.total_residue());

	ObjexxFCL::FArray1D_int cuts(1000,0); // dim1 = serial numbers of cut
	ObjexxFCL::FArray2D_int jumps(2,1000,0); // dim1 = from/to; dim2 = serial numbers of jump
	std::map<int,int> const& peptide_cuts = flags_.peptide_cuts;
	std::map<int,int> const& peptide_anchors = flags_.peptide_anchors;
	const int JUMP_FROM = 1;
	const int JUMP_TO = 2;

	int receptor_ncuts = 0;
	if ( ! flags_.pep_fold_only ) { // receptor = docking mode
		jumps(JUMP_FROM, 1) = flags_.receptor_anchor_pos; // first jump is from receptor anchor (towards first peptide anchor, below)
		cuts( 1 ) = flags_.receptor_last_res();
		receptor_ncuts++;
	}
	std::map<int,int>::const_iterator iter, end;
	for ( iter = peptide_cuts.begin(), end = peptide_cuts.end(); iter != end; ++iter ) {
		cuts( iter->first + receptor_ncuts ) = iter->second;
	}
	// add jump from each anchor to the following anchor (and from receptor, to first peptide anchor, if in docking mode)
	int num_jumps = 0; int max_jump = 0;
	for ( iter = peptide_anchors.begin(), end = peptide_anchors.end(); iter != end; ++iter ) {
		int pep_anchor_num = iter->first;
		int residue = iter->second;
		int cur_jump = pep_anchor_num + receptor_ncuts - 1;
		if ( cur_jump >= 1 ) { // this anchor is destination of either previous anchor (>=2), or of the receptor, if in docking mode
			jumps(JUMP_TO, cur_jump) = residue;
			num_jumps++;
		}
		jumps(JUMP_FROM, cur_jump + 1) = residue; // this anchor is src of jump to next anchor


#if (defined WIN32) && (defined WIN_PYROSETTA)
		max_jump = max( max_jump, cur_jump );
#else
				max_jump = std::max( max_jump, cur_jump );
#endif
	}

	runtime_assert_msg(max_jump == num_jumps,
		"invalid anchor indexing in FlexPepDock parameter file (or in default FlexPepDock parameters)");

	// multichain receptor
	if ( ! flags_.pep_fold_only && flags_.receptor_chain().size() > 1 ) {
		core::pose::PDBInfoCOP pdbinfo = pose.pdb_info();
		Size resi = 1;
		Size chain = 0;
		while ( resi < pose.total_residue() && chain < flags_.receptor_chain().size()-1 ) {
			if ( pdbinfo->chain(resi+1) != flags_.receptor_chain().at(chain) ) {
				int new_jump = ++max_jump;
				jumps(JUMP_FROM, new_jump) = resi;
				jumps(JUMP_TO, new_jump) = resi+1;
				cuts(num_jumps+1) = resi;
				num_jumps++;
				chain++;
			}
			resi++;
		}
	}

	//hack to support multiple ligands besides the peptide
	//assumes ligand(s) is last residue in the pdb.
	if ( flags_.is_ligand_present(pose) ) { // TODO: verify code validity for peptide-folding only (= no receptor)
		core::pose::PDBInfoCOP pdbinfo = pose.pdb_info();
		if ( flags_.receptor_nres() + flags_.peptide_nres() < (int)pose.total_residue() ) {
			cuts ( num_jumps+1 ) = flags_.peptide_last_res();
			Size resi = flags_.receptor_nres() + flags_.peptide_nres() + 1;
			while ( resi <= pose.total_residue() ) {
				char currChain = pdbinfo->chain(resi);
				int ligand_jump = ++max_jump;
				jumps(JUMP_FROM, ligand_jump) = flags_.receptor_anchor_pos; // TODO: this line is probably incompatible with pep_fold_only
				jumps(JUMP_TO, ligand_jump) = resi;
				num_jumps++;
				cuts(num_jumps+1) = resi;
				while ( resi <= pose.total_residue() && pdbinfo->chain(resi) == currChain ) {
					resi++;
				}
			}
		}
	}

	// create and attach fold-tree
	kinematics::FoldTree f;
	f.clear();
	f.tree_from_jumps_and_cuts
		( pose.total_residue(), num_jumps, jumps, cuts );
	TR << "Old FoldTree : " << pose.fold_tree() << std::endl;
	pose.fold_tree(f);
	TR << "New FoldTree : " << pose.fold_tree() << std::endl;
}


////////////////////////////////////////
// set constraints to prevent receptor from changing too much from starting structure
////////////////////////////////////////
void
FlexPepDockingProtocol::set_receptor_constraints( core::pose::Pose & pose )
{
	using namespace core::scoring::constraints;
	using namespace core::chemical;
	using namespace core::conformation;
	using namespace core::id;
	using namespace core::scoring;

	ConstraintSetOP cst_set( new ConstraintSet() );
	core::scoring::func::HarmonicFuncOP spring( new core::scoring::func::HarmonicFunc( 0 /*mean*/, 1 /*std-dev*/) );
	conformation::Conformation const & conformation( pose.conformation() );
	for ( int i=flags_.receptor_first_res();
			i <= flags_.receptor_last_res(); i++ ) {
		// Residue const  & reside = pose.residue( i );
		AtomID CAi ( pose.residue(i).atom_index( " CA " ), i );
		cst_set->add_constraint
			(  ConstraintCOP( ConstraintOP( new CoordinateConstraint
			( CAi, CAi, conformation.xyz( CAi ), spring ) ) )
		);
	}
	pose.constraint_set( cst_set );
	scorefxn_->set_weight(coordinate_constraint, 1.0);
}


/////////////////////////////////////////
// Set movemaps for optimization
// assumes the flags were filled with the pose
// length, so should be invoked only after apply()
//
// if cmd-line -in:file:movemap is used, overrides movemap
// position explicitly set by the user (see MoveMap::init_from_file()
// for more information)
/////////////////////////////////////////
void
FlexPepDockingProtocol::set_allowed_moves( )
{
	using namespace basic::options;

	// rb + all sc torsions + peptide bb)
	movemap_->set_bb_true_range(
		flags_.peptide_first_res(),
		flags_.peptide_last_res());
	movemap_->set_chi(true);

	// rigid-body
	movemap_->set_jump(false);
	movemap_->set_jump( rb_jump_, true );

	// minimizer may also allow receptor bb to move
	movemap_minimizer_=movemap_->clone();
	if ( flags_.min_receptor_bb && ! flags_.pep_fold_only ) {
		for ( int i=flags_.receptor_first_res();
				i <= flags_.receptor_last_res(); i++ ) {
			movemap_minimizer_->set_bb(i, true);
		}
	}

	// overrides movemaps at user custom positions
	if ( option[OptionKeys::in::file::movemap].user() ) {
		movemap_->init_from_file( option[OptionKeys::in::file::movemap] );
		movemap_minimizer_->init_from_file( option[OptionKeys::in::file::movemap] );
	}

}


/////////////////////////////////////////
// Minimize_only function. to use if the
// user raised the flexPepDockMinimizeOnly
// flag
/////////////////////////////////////////
void
FlexPepDockingProtocol::minimize_only(
	core::pose::Pose & pose,
	const std::string & min_type,
	const float min_func_tol
)
{
	protocols::simple_moves::MinMover minimizer(movemap_minimizer_, scorefxn_, min_type, min_func_tol, true /*nb_list*/ );
	minimizer.apply(pose);
}


////////////////////////////////////////////////////////////////////////////////
///
/// @brief prepack protocol for flexpepdock
//         (0)  minimize all side-chains + peptide b.b., just to get a reference to best possible energy
//         (i)   translate apart (by 1000A)
//         (ii)  repack + minimize side-chains of protein
//         (iii)  translate back (by 1000A)
///
//  @details
//  Prepacking a docked structure
//
//  @param
//  pose - the pose to prepack
//  ppk_receptor - whether to prepack the receptor protein
//  ppk_peptide - whether to prepack the lignad peptide
void
FlexPepDockingProtocol::prepack_only(
	pose::Pose & pose, bool ppk_receptor, bool ppk_peptide
)
{
	using namespace moves;
	using namespace basic::options;
	using namespace std;
	using namespace core::pack;

	// set up dofs: all side-chain DOFs except S-S bonds
	core::kinematics::MoveMapOP mm_protein( new core::kinematics::MoveMap );
	mm_protein->clear();
	int const receptor_nres = flags_.receptor_nres();
	core::Size pack_start = (ppk_receptor) ? 1            : receptor_nres+1;
	core::Size pack_end =   (ppk_peptide) ? pose.total_residue() : receptor_nres;
	for ( core::Size i = pack_start; i <= pack_end; ++i ) {
		mm_protein->set_chi(i, true);
	}
	// set up packer
	pack::task::PackerTaskOP task;
	task = pack::task::TaskFactory::create_packer_task( pose );
	task->initialize_from_command_line().restrict_to_repacking();//flags+no-design
	pack::task::operation::NoRepackDisulfides noRepackDisulf;
	noRepackDisulf.apply(pose, *task);
	// allow packing only in the range [pack_start..pack_end]
	for ( core::Size resid = 1; resid < pack_start; resid++ ) {
		task->nonconst_residue_task(resid).prevent_repacking();
	}
	for ( core::Size resid = pack_end+1; resid <= pose.total_residue() ; resid++ ) {
		task->nonconst_residue_task(resid).prevent_repacking();
	}
	// support cmd-line option for unbound rotamers
	pack::rotamer_set::UnboundRotamersOperationOP unboundrot_oper( new pack::rotamer_set::UnboundRotamersOperation() );
	unboundrot_oper->initialize_from_command_line();
	task->append_rotamerset_operation( unboundrot_oper );
	protocols::simple_moves::PackRotamersMoverOP prepack_protein( new protocols::simple_moves::PackRotamersMover( scorefxn_, task ) );

	// set up min mover to pre-minimize the side chains + backbone // TODO: 1e-5 - is this a good threshold? optimize this
	protocols::simple_moves::MinMoverOP min_mover( new protocols::simple_moves::MinMover(mm_protein, scorefxn_, "lbfgs_armijo_nonmonotone", 1e-5, true/*nblist*/, false/*deriv_check*/  ) );

	//set up translate-by-axis movers
	Real trans_magnitude = 1000;
	rigid::RigidBodyTransMoverOP translate_away( new rigid::RigidBodyTransMover( pose, rb_jump_ ) );
	translate_away->step_size( trans_magnitude );
	rigid::RigidBodyTransMoverOP translate_back( new rigid::RigidBodyTransMover( pose, rb_jump_ ) );
	translate_back->step_size( trans_magnitude );
	translate_back->trans_axis().negate();

	// print energy for repack + minimization of original complex, just as a reference
	core::pose::Pose orig_pose( pose );
	min_mover->apply(pose);
	TR.Debug << "Initial score: " << (*scorefxn_)(orig_pose) << endl;
	TR.Debug << "minimized score = " << (*scorefxn_)(pose) << endl;
	pose = orig_pose; // restore

	// run pre-pack protocol
	if ( ! flags_.pep_fold_only ) {
		translate_away->apply(pose);
	}
	prepack_protein->apply(pose);
	min_mover->movemap(mm_protein); // only s.c.
	min_mover->apply(pose); // only s.c.
	if ( ! flags_.pep_fold_only ) {
		translate_back->apply(pose);
	}
}

//////////////////////////////////////////////////
// Performs PCA of the binding site CA coordinates and peptide CA
// coordinates and placing peptide first principle component
// along the binding site first principle component with center
// of masses aligned
//////////////////////////////////////////////////
void
FlexPepDockingProtocol::place_peptide_on_binding_site(
	core::pose::Pose & pose
){
	using namespace core;
	using namespace numeric;
	using namespace utility;
	using namespace basic::options;
	using namespace protocols;

	// reading siteconstraints and storing receptor binding site residues to a vector1 object
	if ( option[ OptionKeys::constraints::cst_file ].user() ) {
		core::scoring::constraints::ConstraintSetOP site_cstset( core::scoring::constraints::ConstraintIO::get_instance()->read_constraints( core::scoring::constraints::get_cst_file_option(), core::scoring::constraints::ConstraintSetOP( new core::scoring::constraints::ConstraintSet ), pose  ) );
		TR << "Read Site Constraints: " << ( site_cstset->has_constraints() ? "YES" : "NONE" ) << std::endl;
		utility::vector1< core::scoring::constraints::ConstraintCOP > site_cstset_list( site_cstset->get_all_constraints() );;

		utility::vector1< Size > site_cst_residues;
		utility::vector1< Size > binding_site_residues;

		for ( utility::vector1< core::scoring::constraints::ConstraintCOP >::const_iterator i = site_cstset_list.begin(), end = site_cstset_list.end(); i != end; ++i ) {
			site_cst_residues = (*i)->residues();
			binding_site_residues.push_back(site_cst_residues[1]);
		}
		for ( utility::vector1< Size >::const_iterator i = binding_site_residues.begin(), end = binding_site_residues.end(); i != end; ++i ) {
			TR << "Binding site residues : " << *i << std::endl;
		}

		// calculating peptide COM and principle components
		utility::vector1< Vector > peptide_c_alpha_coords;
		for ( int i = flags_.peptide_first_res(); i <= flags_.peptide_last_res(); ++i ) {
			peptide_c_alpha_coords.push_back( pose.residue( i ).xyz( "CA" ) );
		}
		Vector peptide_c_alpha_centroid(0, 0, 0);
		Size peptide_n_res = peptide_c_alpha_coords.size();

		for ( Size i = 1; i <= peptide_n_res; ++i ) {
			peptide_c_alpha_centroid += peptide_c_alpha_coords[i];
		}
		peptide_c_alpha_centroid /= peptide_n_res;

		TR << "Peptide CAs Center of Mass : " << peptide_c_alpha_centroid[0] << " " << peptide_c_alpha_centroid[1] << " " << peptide_c_alpha_centroid[2] << std::endl;

		numeric::xyzMatrix< core::Real > peptide_eigenvectors = numeric::principal_components( peptide_c_alpha_coords );
		numeric::xyzVector< core::Real > peptide_first_pc = numeric::first_principal_component( peptide_c_alpha_coords );
		Vector peptide_eigenvalues = numeric::principal_component_eigenvalues( peptide_c_alpha_coords );

		TR << "Peptide CAs First Principle Component : " << peptide_first_pc.x() << " " << peptide_first_pc.y() << " " << peptide_first_pc.z() << std::endl;
		TR << "Peptide CAs Eigenvalues : " << peptide_eigenvalues.x() << " " << peptide_eigenvalues.y() << " " << peptide_eigenvalues.z() << std::endl;

		// calculating binding site COM and principle components
		utility::vector1< Vector > binding_site_c_alpha_coords;
		for ( utility::vector1< Size >::const_iterator i = binding_site_residues.begin(), end = binding_site_residues.end(); i != end; ++i ) {
			binding_site_c_alpha_coords.push_back( pose.residue( *i ).xyz( "CA" ) );
		}
		Vector binding_site_c_alpha_centroid(0, 0, 0);
		Size binding_site_n_res = binding_site_c_alpha_coords.size();

		for ( Size i = 1; i <= binding_site_n_res; ++i ) {
			binding_site_c_alpha_centroid += binding_site_c_alpha_coords[i];
		}
		binding_site_c_alpha_centroid /= binding_site_n_res;

		TR << "Binding Site CAs Center of Mass : " << binding_site_c_alpha_centroid[0] << " " << binding_site_c_alpha_centroid[1] << " " << binding_site_c_alpha_centroid[2] << std::endl;

		numeric::xyzMatrix< core::Real > binding_site_eigenvectors = numeric::principal_components( binding_site_c_alpha_coords );
		numeric::xyzVector< core::Real > binding_site_first_pc = numeric::first_principal_component( binding_site_c_alpha_coords );
		Vector binding_site_eigenvalues = numeric::principal_component_eigenvalues( binding_site_c_alpha_coords );

		TR << "Binding Site CAs First Principle Component : " << binding_site_first_pc.x() << " " << binding_site_first_pc.y() << " " << binding_site_first_pc.z() << std::endl;
		TR << "Binding Site CAs Eigenvalues : " << binding_site_eigenvalues.x() << " " << binding_site_eigenvalues.y() << " " << binding_site_eigenvalues.z() << std::endl;

		// caculating translation vector to align center of masses
		Vector com_align_axis = (peptide_c_alpha_centroid - binding_site_c_alpha_centroid);
		core::Real com_distance = std::sqrt((peptide_c_alpha_centroid[0] - binding_site_c_alpha_centroid[0])*(peptide_c_alpha_centroid[0] - binding_site_c_alpha_centroid[0]) + (peptide_c_alpha_centroid[1] - binding_site_c_alpha_centroid[1])*(peptide_c_alpha_centroid[1] - binding_site_c_alpha_centroid[1]) + (peptide_c_alpha_centroid[2] - binding_site_c_alpha_centroid[2])*(peptide_c_alpha_centroid[2] - binding_site_c_alpha_centroid[2]));
		TR << "COM distance : " << com_distance << std::endl;

		// performing translation to align center of masses
		rigid::RigidBodyTransMoverOP translate_com( new rigid::RigidBodyTransMover( pose, rb_jump_ ) );
		translate_com->trans_axis(com_align_axis);
		translate_com->trans_axis().negate();
		translate_com->step_size( com_distance );
		translate_com->apply(pose);

		// calculating rotation matrix and performing rotation to align principle components
		binding_site_first_pc.x( binding_site_first_pc.x() );
		binding_site_first_pc.y( binding_site_first_pc.y() );
		binding_site_first_pc.z( binding_site_first_pc.z() );
		binding_site_first_pc.normalize_any();

		peptide_first_pc.x( peptide_first_pc.x() );
		peptide_first_pc.y( peptide_first_pc.y() );
		peptide_first_pc.z( peptide_first_pc.z() );
		peptide_first_pc.normalize_any();

		core::Real rotation_angle = numeric::arccos( peptide_first_pc.dot( binding_site_first_pc ) );
		Vector rotation_axis = peptide_first_pc.cross( binding_site_first_pc );

		numeric::xyzMatrix< core::Real > rotation_matrix = numeric::rotation_matrix( rotation_axis, rotation_angle );

		TR << "Performing rotation " << std::endl;

		core::kinematics::Jump flexible_jump = pose.jump( rb_jump_ );
		core::kinematics::Stub upstream_stub = pose.conformation().upstream_jump_stub( rb_jump_ );
		flexible_jump.rotation_by_matrix( upstream_stub, binding_site_c_alpha_centroid, rotation_matrix );
		pose.set_jump( rb_jump_, flexible_jump );

		//applying SlideIntoContact to remove inter-chain clashes
		TR << "Performinh slide into contact " << std::endl;
		SlideIntoContact(pose, com_align_axis);
	}
}


////////////////////////////////////////////
// allow sampling in different pcas and different peptide
// orientations based of binding site eigen values
////////////////////////////////////////////
void FlexPepDockingProtocol::flip_in_pcs( core::pose::Pose & pose )
{
	using namespace std;
	using namespace core;
	using namespace numeric;
	using namespace utility;
	using namespace basic::options;
	using namespace protocols;
	using namespace std;

	Size pcs_num = flags_.sample_pc;
	utility::vector1< Vector > peptide_c_alpha_coords;

	for ( int i = flags_.peptide_first_res(); i <= flags_.peptide_last_res(); ++i ) {
		peptide_c_alpha_coords.push_back( pose.residue( i ).xyz( "CA" ) );
	}

	Vector peptide_c_alpha_centroid(0, 0, 0);
	Size peptide_n_res = peptide_c_alpha_coords.size();

	for ( Size i = 1; i <= peptide_n_res; ++i ) {
		peptide_c_alpha_centroid += peptide_c_alpha_coords[i];
	}
	peptide_c_alpha_centroid /= peptide_n_res;

	TR << "Peptide CAs Center of Mass : " << peptide_c_alpha_centroid[0] << " " << peptide_c_alpha_centroid[1] << " " << peptide_c_alpha_centroid[2] << std::endl;

	numeric::xyzMatrix< core::Real > peptide_eigenvectors = numeric::principal_components( peptide_c_alpha_coords );

	Vector first_pc_axis(peptide_eigenvectors.col_x()[0], peptide_eigenvectors.col_x()[1], peptide_eigenvectors.col_x()[2] );
	Vector second_pc_axis(peptide_eigenvectors.col_y()[0], peptide_eigenvectors.col_y()[1], peptide_eigenvectors.col_y()[2] );
	Vector third_pc_axis(peptide_eigenvectors.col_z()[0], peptide_eigenvectors.col_z()[1], peptide_eigenvectors.col_z()[2] );

	if ( pcs_num == 1 ) {
		Size rand_num = (Size)(numeric::random::uniform() + 0.5);
		float rot_angle = 180.0*(rand_num);;
		TR << "Flipping the peptide " << std::endl;
		rigid::RigidBodyDeterministicSpinMover spin_mover ( rb_jump_, second_pc_axis, peptide_c_alpha_centroid, rot_angle );
		spin_mover.apply(pose);
	}

	if ( pcs_num == 2 ) {
		Size rand_num = (Size)(numeric::random::random_range(1,4) + 0.5);
		float rot_angle = 90.0*(rand_num);
		TR << "Flipping the peptide " << std::endl;
		rigid::RigidBodyDeterministicSpinMover spin_mover ( rb_jump_, second_pc_axis, peptide_c_alpha_centroid, rot_angle );
		spin_mover.apply(pose);
	}

}


///////////////////////////////////////////////////
// SlideIntoContact - moving the partners toward or away from
// from each other to remove interchain clashes
////////////////////////////////////////////////////

void FlexPepDockingProtocol::SlideIntoContact( core::pose::Pose & pose, core::Vector translate_axis )
{
	using namespace moves;
	using namespace core::scoring;

	core::scoring::ScoreFunctionOP scorefxn_slide_ = ScoreFunctionFactory::create_score_function( "interchain_cen" );

	rigid::RigidBodyTransMoverOP translate_mover( new rigid::RigidBodyTransMover( pose, rb_jump_ ) );
	translate_mover->step_size(1.0);
	translate_mover->trans_axis(translate_axis);

	core::Size counter( 0 );
	core::Size const counter_breakpoint( 500 );

	( *scorefxn_slide_ )( pose );

	TR << "Moving away from each other" << std::endl;
	while ( pose.energies().total_energies()[ interchain_vdw ] > 0.1 && counter <= counter_breakpoint ) {
		translate_mover->apply( pose );
		( *scorefxn_slide_ )( pose );
		++counter;
	}
	if ( counter > counter_breakpoint ) {
		TR << "failed moving away with original vector. Aborting SlideIntoContact." << std::endl;
	}

	TR << "Moving partners towards each other" << std::endl;
	counter = 0;
	TR << "Moving together" << std::endl;
	translate_mover->trans_axis().negate();
	while ( pose.energies().total_energies()[ interchain_vdw ] < 0.1 && counter <= counter_breakpoint  ) {
		translate_mover->apply( pose );
		( *scorefxn_slide_ )( pose );
		++counter;
	}
	if ( counter > counter_breakpoint ) {
		TR<<"moving together failed. Aborting DockingSlideIntoContact."<<std::endl;
	}

}


/////////////////////////////////////////
// randomly perturb phi-psi of peptide to
// a given range. *also assumes params ia read
/////////////////////////////////////////
void
FlexPepDockingProtocol::random_peptide_phi_psi_perturbation(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace numeric;

	double pertSize = flags_.random_phi_psi_pert_size;

	for ( int i=flags_.peptide_first_res();
			i < flags_.peptide_first_res()+flags_.peptide_nres(); i++ ) {
		double phi_offset = numeric::random::uniform()*(pertSize)*2 - pertSize;
		double psi_offset = numeric::random::uniform()*(pertSize)*2 - pertSize;
		double new_phi = principal_angle_degrees( pose.phi(i) + phi_offset );
		double new_psi = principal_angle_degrees( pose.psi(i) + psi_offset );
		pose.set_phi(i,new_phi);
		pose.set_psi(i,new_psi);
	}
}


/////////////////////////////////////////
// extend peptide setting all phi/psi to
// +- 135 deg. assumes peptide dofs were defined.
/////////////////////////////////////////
void
FlexPepDockingProtocol::extend_peptide(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace numeric;

	for ( int i=flags_.peptide_first_res();
			i < flags_.peptide_first_res() + flags_.peptide_nres(); i++ ) {
		pose.set_phi(i,-135);
		pose.set_psi(i,135);
	}
}


/////////////////////////////////////////
void
FlexPepDockingProtocol::random_rb_pert(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace moves;

	float current_score = (*scorefxn_)( pose );
	float new_score;
	core::pose::Pose tmpPose;
	// TODO: sepearate rotation and translation flags
	rigid::RigidBodyPerturbMoverOP rb_mover( new rigid::RigidBodyPerturbMover(
		rb_jump_ /*jump_num*/,
		flags_.rb_rot_size /*rot*/,
		flags_.rb_trans_size /*trans*/ ) );
	do {
		tmpPose = pose;
		rb_mover->apply(tmpPose);
		new_score = (*scorefxn_)( tmpPose );
	} while ((new_score-current_score) > 3000);
	pose = tmpPose;
}


/////////////////////////////////////////
// perform small moves on the peptide
/////////////////////////////////////////
void
FlexPepDockingProtocol::small_moves(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace core::pose;
	using namespace protocols;

	simple_moves::SmallMoverOP small_mover( new simple_moves::SmallMover( movemap_, 0.8 /*temp*/, 100 /*nmoves ???*/ ) );
	small_mover->angle_max('L',flags_.smove_angle_range);
	small_mover->apply(pose);

}


/////////////////////////////////////////
// perform sheer moves on the peptide
/////////////////////////////////////////
void
FlexPepDockingProtocol::shear_moves(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace core::pose;
	using namespace protocols;

	simple_moves::ShearMoverOP shear_mover( new simple_moves::ShearMover( movemap_, 0.8 /*temp*/, 100 /*nmoves ???*/ ) );
	shear_mover->angle_max('L',flags_.smove_angle_range);
	shear_mover->apply(pose);

}


/////////////////////////////////////////
// backrub the peptide
/////////////////////////////////////////
void
FlexPepDockingProtocol::backrub_move(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace core::pose;
	using namespace protocols;

	// set up the BackrubMover
	protocols::backrub::BackrubMover backrubmover;
	// read known and unknown optimization parameters from the database
	// ? backrubmover.branchopt().read_database();
	backrubmover.clear_segments();
	backrubmover.set_input_pose(pose.get_self_ptr());

	// determine list of residues to backrub
	utility::vector1<core::Size> resnums;
	for ( int i=flags_.peptide_first_res();
			i < flags_.peptide_first_res() + flags_.peptide_nres(); i++ ) {
		resnums.push_back(i);
	}

	// add segments to the backrub mover
	backrubmover.add_mainchain_segments(resnums, utility::vector1<std::string>(1, "CA"), 3 , 34 );
	//values adaptd from bacrub.cc
	TR << "backrub: added mainchain segments" << endl;
	// ? backrubmover.optimize_branch_angles(pose);
	backrubmover.apply(pose);
	TR << "backrub: applied" << endl;
}


/////////////////////////////////////////
// change sequence of peptide to polyAla
/////////////////////////////////////////
void
FlexPepDockingProtocol::polyAla(
	core::pose::Pose & pose
)
{
	using namespace std;
	using namespace core::pose;
	using namespace protocols;
	using namespace core::conformation;
	using namespace core::chemical;

	ResidueTypeSetCOP residue_set( ChemicalManager::get_instance()->residue_type_set( "fa_standard" ) );
	//   ResidueTypeSet const & residue_set ( pose.residue(1).residue_type_set() );
	ResidueType const & restype( residue_set->name_map( "ALA" ) );
	ResidueOP ala;
	for ( int i=flags_.peptide_first_res();
			i < flags_.peptide_first_res() + flags_.peptide_nres(); i++ ) {
		ala = ResidueFactory::create_residue(restype);// ,pose.residue(i),pose.conformation(),true/*preserve CB*/);
		pose.replace_residue(i,*ala,true);
	}
}

/////////////////////////////////////////
// TODO: think about filters in the future - make more serious decisions
//       on how to use them
// check if the given pose passes the filters imposed by the cmd line
/////////////////////////////////////////
bool
FlexPepDockingProtocol::check_filters( core::pose::Pose & pose) {
	using namespace std;
	bool passed = true;
	//if no filters were turned on during the run filters are passed
	if ( flags_.score_filter!=10000 ) { // TODO: make good indication of whether the user specified score_filter (requires some changes into flags_ infrastructure, in order to use basic::options::user()
		TR << "Applying score filter " << flags_.score_filter << endl;
		passed = ((*scorefxn_)(pose) < flags_.score_filter);
	}
	if ( flags_.hb_filter!=0 ) {
		//TODO calc HB number: passed = passed && hb
	}
	if ( flags_.hotspot_filter!=0 ) {
		//TODO calc number of hotspots: passed = passed && hotspots
	}

	return passed;
}

/////////////////////////////////////////
// Randomly move peptides cutpoint in
// order to not bias the sampling
// of torsion angles.
/////////////////////////////////////////
void
FlexPepDockingProtocol::randomlySlidePeptideJump(core::pose::Pose & pose){

	using namespace core::kinematics;
	using namespace numeric;
	Size new_rand_cut = (Size)(numeric::random::uniform()*(flags_.peptide_nres()));
	FoldTree f = pose.fold_tree();
	f.slide_jump(rb_jump_, //along which jump
		f.upstream_jump_residue(rb_jump_),//keep prot anchor
		flags_.peptide_first_res() + new_rand_cut ); //change pep cutpoint
	TR << "Moved peptide cutpoint to " <<  flags_.peptide_first_res() + new_rand_cut << std::endl;
}


/////////////////////////////////////////
// torsions (small/shear) MCM
/////////////////////////////////////////
void
FlexPepDockingProtocol::torsions_monte_carlo_minimize(
	core::pose::Pose & pose,
	const int cycles,
	const std::string & min_type,
	const float minimization_threshold,
	const float min_func_tol
)
{
	using namespace std;
	using namespace core::pose;
	using namespace core::kinematics;
	using namespace moves;

	// create a monte_carlo object
	const float temperature ( 0.8 ); // TODO: tune param
	moves::MonteCarloOP mc( new moves::MonteCarlo (pose, *scorefxn_,temperature) );
	int n_accepted = 0; // diagnostic counter
	const float rt_energycut = 0.05; // TODO: tune param

	protocols::simple_moves::EnergyCutRotamerTrialsMover rottrial_ecut(scorefxn_, allprotein_tf_, mc, rt_energycut);
	protocols::simple_moves::RotamerTrialsMinMover interface_rtmin(scorefxn_, interface_tf_);
	protocols::simple_moves::MinMover minimizer(movemap_minimizer_, scorefxn_, min_type, min_func_tol, true /*nb_list*/ );

	// start the minimization
	for ( int i=1; i<=cycles; ++i ) {

		if ( i % 2 == 0 ) {
			small_moves(pose);
		} else {
			shear_moves(pose);
		}
		if ( i % 8 == 0 || i == cycles ) {
			interface_packer_->apply( pose );
		} else {
			//rotamer_trials, but for the whole protein
			rottrial_ecut.apply(pose); // allow non-interface residues as well?
		}
		const float current_score = (*scorefxn_)( pose );
		const float lowest_score = mc->lowest_score();
		if ( current_score - lowest_score < minimization_threshold ) {
			minimizer.apply(pose);
		}
		const bool accepted = mc->boltzmann( pose );
		if ( accepted ) {
			++n_accepted;
		}
	}
	TR.Debug << "torsion-MCM --" << n_accepted << " out of " << cycles
		<< " monte carlo cycles accepted" << endl;
	pose = mc->lowest_score_pose();
}

////////////////////////////////////////////////////////////////////////////
//
// @brief
// Rigid body optimization session using Monte-Carlo with Energy Minimization
//
// @param pose[in,out]        Pose to be optimized
// @param cycles[in]          Number of MCM cycles
// @param min_type[in]        Type of energy minimization method
// @param trans_magnitude[in] Translation magnitude in each perturbation
//                            (Angstroms)
// @param rot_magnitude[in]   Rotation magnitude in each perturbation
//                            (Degrees)
// @param minimization_threshold  Minimize only structures that "stand a chance" - energy > best.energy + min_thresh // TODO: do we want this filter? How much running time does it save?
//
////////////////////////////////////////////////////////////////////////////
void
FlexPepDockingProtocol::rigidbody_monte_carlo_minimize(
	core::pose::Pose & pose,
	const int cycles,
	const std::string & min_type,
	const float trans_magnitude,
	const float rot_magnitude,
	const float minimization_threshold,
	const float min_func_tol
)
{
	using namespace std;
	using namespace core::pose;
	using namespace core::kinematics;
	using namespace moves;

	// create a monte_carlo object
	const float temperature ( 0.8 ); // TODO: tune param
	moves::MonteCarloOP mc( new moves::MonteCarlo (pose, *scorefxn_,temperature) );
	int n_accepted = 0; // diagnostic counter
	const float rt_energycut = 0.05; // TODO: tune param

	rigid::RigidBodyPerturbMover rb_mover(
		rb_jump_, rot_magnitude, trans_magnitude );
	protocols::simple_moves::RotamerTrialsMover interface_rottrial(scorefxn_, interface_tf_);
	protocols::simple_moves::EnergyCutRotamerTrialsMover rottrial_ecut(scorefxn_, allprotein_tf_, mc, rt_energycut);
	protocols::simple_moves::RotamerTrialsMinMover interface_rtmin(scorefxn_, interface_tf_);
	protocols::simple_moves::MinMover minimizer(
		movemap_minimizer_, scorefxn_, min_type, min_func_tol, true /*nb_list*/ );

	// start the minimization
	for ( int i=1; i<=cycles; ++i ) {
		rb_mover.apply(pose);
		if ( i % 8 == 0 || i == cycles ) { // tune param 8
			interface_packer_->apply( pose );
		} else {
			//rotamer_trials but for the whole protein
			rottrial_ecut.apply(pose);
		}
		const float current_score = (*scorefxn_)( pose );
		const float lowest_score = mc->lowest_score();
		if ( current_score - lowest_score < minimization_threshold ) {
			minimizer.apply(pose);
		}
		const bool accepted = mc->boltzmann( pose );
		if ( accepted ) {
			++n_accepted;
		}
	}
	TR.Debug << "rigid-body-MCM --" << n_accepted << " out of " << cycles
		<< " monte carlo cycles accepted" << endl;
	pose = mc->lowest_score_pose();
}


////////////////////////////////////////////////////////////////////////////
//
// @brief
// Take a radnom subset of the peptide and send to loop modeling
// TODO: add this to centroid mode optimization as well?
//
// @param pose[in,out]        Pose to be optimized
//
////////////////////////////////////////////////////////////////////////////
void
FlexPepDockingProtocol::peptide_random_loop_model(
	core::pose::Pose & pose
)
{
	if ( flags_.peptide_nres() < 5 ) { // TODO: 3 is minimum for KIC loop closure, and flanks should be excluded
		return;
	}
	// set up and model a random loop
	Size first_res =  flags_.peptide_first_res(); // TODO: make sure it is ok to use the N' residue
	Size last_res = flags_.peptide_last_res() - 1; // TODO: add the C' terminal, Dan sais there might be a bug with C' followed by another chain
	protocols::loops::LoopsOP loops( new protocols::loops::Loops() );
	loops->add_loop(first_res, last_res); // TODO: cut defaults to zero, is this a random cut?
	for ( Size i = first_res; i <= last_res ; i++ ) {
		runtime_assert( movemap_->get_bb(i) ); // verify loop is movable, this should have been always true for the peptide
	}
	loop_relax_mover_->loops( loops );
	loop_relax_mover_->apply( pose );
}


//////////////////////////////////////////////////////////////////////////////////
// @brief
// The high-resolution protocol for docking a flexible peptide
// onto a globular protein
//
// @param pose[in,out] an input pose conformation to be optimized
void
FlexPepDockingProtocol::hires_fpdock_protocol(pose::Pose& pose)
{
	using namespace core::scoring;
	using namespace moves;
	using utility::file::FileName;
	using namespace std;

	core::scoring::ScoreFunctionOP orig_scorefxn = scorefxn_->clone();

	float rb_trans_mag = 0.2; // in angstroms. more repulsion = less perturb.
	float rb_rot_mag = 7 ; // in degrees.
	// params for ramping of fa_rep / fa_atr / rama scores:
	int rep_ramp_cycles = flags_.rep_ramp_cycles;
	float origw_fa_atr = scorefxn_->get_weight( fa_atr );
	float origw_fa_rep = scorefxn_->get_weight( fa_rep );
	float origw_rama = scorefxn_->get_weight( rama );
	float rep_ramp_step = ( origw_fa_rep - 0.02 ) / float(rep_ramp_cycles-1) ;
	float rama_ramp_step = (origw_rama - 0.01) / float(rep_ramp_cycles-1) ;

	// design operation settings:
	/*if(flags_.design_peptide){
	pack::task::TaskFactoryOP design_tf( new pack::task::TaskFactory );
	if(! flags_.pep_fold_only)
	{
	receptor_protector_oper_ = core::pack::task::operation::RestrictResidueToRepackingOP( new core::pack::task::operation::RestrictResidueToRepacking() );
	receptor_protector_oper_->clear();
	for(int i = flags_.receptor_first_res();
	i <= flags_.receptor_last_res(); ++i){
	receptor_protector_oper_->include_residue(i);
	}
	design_tf->push_back(receptor_protector_oper_);
	design_mover_->task_factory(design_tf);
	TR << "Disabling receptor design" << endl;
	}
	}*/

	// start protocol:
	interface_packer_->apply( pose ); // initial sidechains repack
	for ( int i = 1; i <= rep_ramp_cycles; i++ ) {
		//randomlySlidePeptideJump(pose);
		if ( flags_.boost_fa_atr ) { // ramping fa_atr
			scorefxn_->set_weight
				( fa_atr, origw_fa_atr*(1 + 0.25*(rep_ramp_cycles-i) ) );
		}
		if ( flags_.ramp_fa_rep ) { // ramping fa_rep
			float rep_weight = (0.02 + rep_ramp_step * float(i-1));
			scorefxn_->set_weight( fa_rep, rep_weight );
		}
		// from soft to strong rama
		if ( flags_.ramp_rama ) { // ramping rama
			float rama_weight = (0.01 + rama_ramp_step * float(i-1));
			scorefxn_->set_weight( rama, rama_weight );
		}
		/*  if (flags_.design_peptide) {
		TR.Debug << "Sequence - Before design: " << pose.sequence() << endl;
		TR.Debug << "Peptide sequence: " << pose.chain_sequence(2) << endl;
		TR.Debug << "Designing " << i << endl;
		design_mover_->apply( pose );
		TR.Debug << "Sequence - After design: " << pose.sequence() << endl;
		TR.Debug << "Peptide sequence: " << pose.chain_sequence(2) << endl;
		}
		*/
		if ( flags_.rbMCM  && ! flags_.pep_fold_only ) {
			rigidbody_monte_carlo_minimize(
				pose, flags_.mcm_cycles , "lbfgs_armijo_atol", /* TODO: switch to dfpmin_armijo_nonmonotone_atol, or by flag*/
				rb_trans_mag, rb_rot_mag,
				15.0/*min_thresh*/, 1.0 /*tolerance*/ );
		}
		if ( flags_.torsionsMCM ) {
			torsions_monte_carlo_minimize(
				pose, flags_.mcm_cycles ,
				"lbfgs_armijo_atol", 15.0/*min_thresh*/, /* TODO: switch to dfpmin_armijo_nonmonotone_atol, or by flag */
				1.0 /*tolerance*/ ); // tolerance of 0.1?
		}
		if ( flags_.peptide_loop_model ) {
			peptide_random_loop_model( pose );
		}
	} // end of ramping cycles
	scorefxn_ = orig_scorefxn->clone(); // restore (note that this removes the coordinate constraints weight set above)

	// final minimization, with stringent tolerance
	protocols::simple_moves::MinMover minimizer(movemap_minimizer_, scorefxn_, "lbfgs_armijo_atol", 0.001, true /*nb_list*/ );
	minimizer.apply(pose);
	// final scoring, w/o constraints for receptor backbone
	if ( flags_.min_receptor_bb ) {
		scorefxn_->set_weight( coordinate_constraint, 0.0 ); // TODO: if the user supplied more coordinate constraints from cmd-line, this will bug the final score
	}
	(*scorefxn_)(pose);

}


//////////////////////////////////////////////////////////////////////////////
//
// @brief main flexible peptide docking protocol
//
// @details
// Main function for peptide docking. Includes the following steps:
//      1) Optional centroid mode docking, with alternating MCM
//         cycles of rigid-body movement and peptide torsion relaxation
//      2) High-resolution peptide docking, with alternating MCM
//         cycles of rigid-body movement and peptide torsion relaxation
//         with side-chain repacking on-the-fly
//    OR:
//      Prepack mode: prepare a starting structure for later runs
//   OR:
//      Score only mode
//    OR:
//      Minimize only mode
//
// IMPORTANT NOTE:
// Apply should not change the internal state of this object
// in a way that will affect the behaviour of subsequent apply() invocations
// (because, e.g., the job distributor may invoke apply()
//  for multiple decoys, using the same mover object)
//
//////////////////////////////////////////////////////////////////////////////
void
FlexPepDockingProtocol::apply( pose::Pose & pose )
{
	using namespace scoring;
	using namespace viewer;
	using namespace std;
	using namespace core;
	using namespace core::pose;

	TR << "apply" << std::endl;

	// if no native provided, use pose as pseudo-native reference
	if ( get_native_pose().get() == NULL ) {
		set_native_pose( PoseCOP( PoseOP( new pose::Pose(pose) ) ) ); // TODO: debug this - will it work for PoseCOP;
		TR.Warning << "WARNING: No native supplied by used - using starting structure as reference for statistics" << std::endl;
	}

	kinematics::FoldTree old_foldTree = pose.fold_tree(); // original fold tree should be restored in the end

	// protocols::moves::AddPyMolObserver(pose);
	flags_.updateChains(pose); // validate chain boundaries and letters
	if ( !flags_.valid_anchors() ) {
		flags_.setDefaultAnchors(pose);
	}
	this->setup_foldtree( pose );
	fpdock_metrics_.set_flags(flags_.clone()); // TODO: a somewhat ugly hack
	// TODO: is it proper that an apply() method alters the fold-tree of a pose?
	if ( view_ ) {
		add_conformation_viewer
			( pose.conformation(), "start_pose", 450, 450 , true);
	}

	// initial manupulation
	pose::Pose start_pose_4stats;
	if ( flags_.valid_ref_start_struct() ) {
		core::import_pose::pose_from_file(start_pose_4stats, flags_.ref_start_struct(), core::import_pose::PDB_file);
	} else {
		start_pose_4stats = pose;
	}
	pose::Pose pose_after_lowres = pose;
	set_allowed_moves(); //set the allowed dofs in the system
	core::scoring::constraints::add_fa_constraints_from_cmdline(pose, *scorefxn_);
	core::scoring::constraints::add_constraints_from_cmdline(pose, *scorefxn_lowres_);

	if ( flags_.min_receptor_bb ) {
		set_receptor_constraints(pose);
	}

	// initial peptide manipulations

	if ( flags_.place_peptide ) {
		place_peptide_on_binding_site(pose);
	}

	if ( flags_.sample_pc ) {
		flip_in_pcs(pose);
	}

	if ( flags_.extend ) {
		extend_peptide(pose);
	}
	if ( flags_.random_phi_psi_pert ) {
		random_peptide_phi_psi_perturbation(pose);
	}

	if ( flags_.randomRBstart ) {
		random_rb_pert(pose);
	}

	if ( flags_.backrub_opt ) {
		TR << "doing backrub! " << std::endl;
		backrub_move(pose);
	}

	if ( ( flags_.recal_foldtree && flags_.randomRBstart ) || ( flags_.recal_foldtree && flags_.sample_pc ) ||
			( flags_.recal_foldtree && flags_.random_phi_psi_pert ) ) {
		flags_.setDefaultAnchors(pose);
		this->setup_foldtree( pose );
		std::cout  << "Receptor anchor : " << flags_.receptor_anchor_pos << std::endl;
		std::cout  << "Peptide anchor : " << flags_.peptide_anchors[1] << std::endl;
	}

	if ( flags_.slideintocontact ) {
		Vector anchors_axis_ = pose.residue( flags_.peptide_anchors[1] ).xyz( "CA" ) - pose.residue( flags_.receptor_anchor_pos ).xyz( "CA" );
		SlideIntoContact(pose, anchors_axis_);
	}

	// Mutually exclusive protocols:
	if ( flags_.ppk_only ) {
		prepack_only(pose, !flags_.no_prepack1/*ppk_receptor*/, !flags_.no_prepack2/*ppk_peptide*/);
	} else if ( flags_.min_only ) {
		minimize_only(pose,"lbfgs_armijo_atol", 0.01/*tolerance*/);
	} else if ( flags_.torsionsMCM || flags_.rbMCM || flags_.lowres_preoptimize || flags_.lowres_abinitio ) {
		// Main protocol:
		bool passed_filter = false;
		core::pose::Pose current_pose = pose;
		while ( !passed_filter ) {
			if ( flags_.lowres_abinitio ) {
				FlexPepDockingAbInitio abinitioDocking
					(flags_, scorefxn_lowres_, movemap_, rb_jump_);
				abinitioDocking.apply(pose);
				pose_after_lowres = pose;
			}
			if ( flags_.lowres_preoptimize ) {
				FlexPepDockingLowRes lowresDocking
					(flags_, scorefxn_lowres_, movemap_, rb_jump_);
				lowresDocking.apply(pose);
				// pack peptide since backbone may have changed
				prepack_only(pose, false/*ppk_receptor*/, true/*ppk_peptide*/);
				pose_after_lowres = pose;
			}
			if ( (flags_.pep_refine || flags_.rbMCM || flags_.torsionsMCM)
					|| flags_.design_peptide || flags_.peptide_loop_model ) {
				//Insert cst from cmd-line - uses these flags to operate:
				//option[ OptionKeys::constraints::cst_fa_file ].user()
				//option[ OptionKeys::constraints::cst_fa_weight ].user()
				// weight is applied to:
				// atom_pair_constraint, angle_constraint, dihedral_constraint, coordinate_constraint.
				hires_fpdock_protocol(pose);
			}
			if ( ! flags_.pep_fold_only ) {
				if_metrics_ = fpdock_metrics_.calc_interface_metrics(pose,rb_jump_,scorefxn_); // interface metrics
			}
			passed_filter = check_filters(pose);
			if ( !passed_filter ) {
				pose = current_pose;
				TR << "Failed filters - trying again" << std::endl; // TODO: make it try again
			}
		}
	}
	// output statistics:
	storeJobStatistics(start_pose_4stats, pose_after_lowres, pose);
	basic::prof_show();

	pose.fold_tree(old_foldTree);
	TR.Debug << "fold tree restored: " << pose.fold_tree() << std::endl;
} // END: FlexPepDockingProtocol::apply


std::string
FlexPepDockingProtocol::get_name() const {
	return "FlexPepDockingProtocol";
}


//////////////////////////////////////////////////////////////////////////////////
// @brief mark the peptide residues in the native structure interface
//        If no receptor - trivially mark all residues
//
// @param superpos_partner[out]
//        An array of positions - set to true for peptide residues
// @param native_interface_residues[out]
//        An array of positions - set to true for interface peptide residues
void
FlexPepDockingProtocol::markNativeInterface
( ObjexxFCL::FArray1D_bool& superpos_partner,
	ObjexxFCL::FArray1D_bool& native_interface_residues) const
{
	pose::Pose native_pose;
	protocols::scoring::InterfaceOP native_interface;
	native_pose=*get_native_pose();
	// find interface
	if ( ! flags_.pep_fold_only ) { // docking mode
		native_interface
			= protocols::scoring::InterfaceOP( new protocols::scoring::Interface(rb_jump_) );
		native_interface->distance(8); // between CB atoms, CA for Gly (to be precise, the anchor atom is set within the mini DB)
		native_pose.update_residue_neighbors();
		native_interface->calculate( native_pose );
	}
	// mark interface and peptide residues
	for ( int i=flags_.peptide_first_res();
			i <= flags_.peptide_last_res(); i++ ) {
		superpos_partner(i)=true;
		if ( flags_.pep_fold_only ) { // not docking mode
			native_interface_residues(i) = true;
		} else if ( native_interface->is_interface(i) ) { // docking mode + interface res
			TR.Debug << "Peptide residue " << i-flags_.peptide_first_res()+1
				<< " in interface" << std::endl;
			native_interface_residues(i)=true;
		}
	}
}

void
FlexPepDockingProtocol::addLowResStatistics
( core::pose::Pose const& start_pose,
	core::pose::Pose& pose_after_lowres ) const
{
	using namespace scoring;
	using protocols::jd2::JobDistributor;

	if ( !get_native_pose() ) {
		TR.Warning << "WARNING: missing '-native' flag, skipping statistics on RMSD to native" << std::endl;
		return;
	}

	// Score statistics are added to current job
	protocols::jd2::JobOP cur_job( JobDistributor::get_instance()->current_job() );
	// switch residue set to centroid if necessary
	protocols::simple_moves::SwitchResidueTypeSetMover
		to_centroid_mover( core::chemical::CENTROID );
	pose::Pose start_pose_CEN = start_pose;
	pose::Pose pose_after_lowres_CEN = pose_after_lowres;
	pose::Pose native_CEN = *get_native_pose();
	if ( start_pose_CEN.is_fullatom() ) { // TODO: actually the if is not necessary
		to_centroid_mover.apply(start_pose_CEN);
	}
	if ( pose_after_lowres_CEN.is_fullatom() ) {
		to_centroid_mover.apply(pose_after_lowres_CEN);
	}
	if ( native_CEN.is_fullatom() ) {
		to_centroid_mover.apply(native_CEN);
	}
	// add score statistics
	cur_job->add_string_real_pair( "score_lowres_start",
		(*scorefxn_lowres_)(start_pose_CEN) );
	cur_job->add_string_real_pair( "score_lowres_opt",
		(*scorefxn_lowres_)(pose_after_lowres_CEN) );
	// mark peptide residues and interface (computed from native)
	FArray1D_bool superpos_partner ( native_CEN.total_residue(), false );
	FArray1D_bool native_interface_residues ( native_CEN.total_residue(), false );
	markNativeInterface(superpos_partner, native_interface_residues);
	// add all peptide RMSD:
	using core::scoring::rmsd_with_super;
	using core::scoring::rmsd_no_super_subset;
	using core::scoring::is_protein_CA;
	using core::scoring::is_protein_backbone;
	if ( !flags_.pep_fold_only ) {
		cur_job->add_string_real_pair( "rmsCA_lowres",
			rmsd_no_super_subset( native_CEN, pose_after_lowres_CEN, superpos_partner, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB_lowres",
			rmsd_no_super_subset( native_CEN, pose_after_lowres_CEN, superpos_partner, is_protein_backbone ) );
		// add peptide interface-only RMSD:
		cur_job->add_string_real_pair( "rmsCA_intrf_lowres",
			rmsd_no_super_subset( native_CEN, pose_after_lowres_CEN, native_interface_residues, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB_intrf_lowres",
			rmsd_no_super_subset( native_CEN, pose_after_lowres_CEN, native_interface_residues, is_protein_backbone ) );
	} else if ( flags_.pep_fold_only ) {
		cur_job->add_string_real_pair( "rmsCA_lowres",
			rmsd_with_super( native_CEN, pose_after_lowres_CEN, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB_lowres",
			rmsd_with_super( native_CEN, pose_after_lowres_CEN, is_protein_backbone ) );
	}
}


//////////////////////////////////////////////////////////////////////////////////
// @brief
// make statistics comparing the start pose, the final pose and the native
// and updated final_pose with statistics
//
// @param[in] start_pose - the initial pose sent to this->apply()
// @param[in,out] final_pose - the final pose optimized by this->apply()
//
void
FlexPepDockingProtocol::storeJobStatistics
( pose::Pose const& start_pose,
	pose::Pose& pose_after_lowres,
	pose::Pose& final_pose )
{
	using namespace scoring;
	using protocols::jd2::JobDistributor;

	// Score statistics are added to current job
	protocols::jd2::JobOP cur_job( JobDistributor::get_instance()->current_job() );
	// interface_metrics for docking mode:
	if ( ! flags_.pep_fold_only ) {
		if_metrics_ = fpdock_metrics_.calc_interface_metrics(final_pose,rb_jump_,scorefxn_); // if score only this would be performed only once
		cur_job->add_string_real_pair( "I_sc", if_metrics_.find("I_sc")->second );
		cur_job->add_string_real_pair( "I_bsa", if_metrics_.find("I_bsa")->second );
		cur_job->add_string_real_pair( "I_hb", if_metrics_.find("I_hb")->second );
		cur_job->add_string_real_pair( "I_pack", if_metrics_.find("I_pack")->second );
		cur_job->add_string_real_pair( "I_unsat", if_metrics_.find("I_unsat")->second );
	}

	// peptide scores with / without reference energy
	Real pepScore, pepScore_noref, totalScore;
	totalScore = (*scorefxn_)(final_pose); // making sure energies are up-to-date
	fpdock_metrics_.calc_pep_scores(final_pose, pepScore, pepScore_noref);
	cur_job->add_string_real_pair( "pep_sc", pepScore );
	cur_job->add_string_real_pair( "pep_sc_noref", pepScore_noref );

	// reweighted score (total score + interface score + peptide score)
	cur_job->add_string_real_pair( "reweighted_sc", totalScore + pepScore + if_metrics_.find("I_sc")->second );

	// Comparisons to native metrics:
	if ( !get_native_pose() ) {
		TR.Warning << "WARNING: missing '-native' flag, skipping statistics on RMSD to native" << std::endl;
		return;
	}
	// calculate and store the rms of the starting structure to the native
	pose::Pose native_pose( *get_native_pose() );
	FArray1D_bool superpos_partner ( native_pose.total_residue(), false );
	FArray1D_bool native_interface_residues ( native_pose.total_residue(), false );
	markNativeInterface(superpos_partner, native_interface_residues);


	// start pose statistics:
	using core::scoring::rmsd_with_super;
	using core::scoring::rmsd_no_super_subset;
	using core::scoring::is_protein_CA;
	using core::scoring::is_protein_backbone;
	using core::scoring::is_polymer_heavyatom;

	if ( ! flags_.pep_fold_only ) {
		cur_job->add_string_real_pair( "startRMSca",
			rmsd_no_super_subset( native_pose, start_pose, superpos_partner, is_protein_CA ) );
		cur_job->add_string_real_pair( "startRMSbb",
			rmsd_no_super_subset( native_pose, start_pose, superpos_partner, is_protein_backbone ) );
		cur_job->add_string_real_pair( "startRMSall",
			rmsd_no_super_subset( native_pose, start_pose, superpos_partner, is_polymer_heavyatom ) );
		cur_job->add_string_real_pair( "startRMSallif",
			rmsd_no_super_subset( native_pose, start_pose, native_interface_residues, is_polymer_heavyatom ) );

		// final pose statistics - all peptide residues:
		cur_job->add_string_real_pair( "rmsCA",
			rmsd_no_super_subset( native_pose, final_pose, superpos_partner, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB",
			rmsd_no_super_subset( native_pose, final_pose, superpos_partner, is_protein_backbone ) );
		if ( ! flags_.design_peptide ) {
			cur_job->add_string_real_pair( "rmsALL",
				rmsd_no_super_subset( native_pose, final_pose, superpos_partner, is_polymer_heavyatom ) );
		} else {
			cur_job->add_string_real_pair( "rmsALL", -1.0 ); // invalid RMS on all atoms in design mode
		}
		// final pose statistics - peptide interface residues only:
		cur_job->add_string_real_pair( "rmsCA_if",
			rmsd_no_super_subset( native_pose, final_pose, native_interface_residues, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB_if",
			rmsd_no_super_subset( native_pose, final_pose, native_interface_residues, is_protein_backbone ) );
		if ( ! flags_.design_peptide ) {
			cur_job->add_string_real_pair( "rmsALL_if",
				rmsd_no_super_subset( native_pose, final_pose, native_interface_residues, is_polymer_heavyatom ) );
		} else {
			cur_job->add_string_real_pair( "rmsALL_if", -1.0 ); // invalid RMS on all atoms in design mode
		}
	}

	if ( flags_.pep_fold_only ) {
		cur_job->add_string_real_pair( "startRMSca",
			rmsd_with_super( native_pose, start_pose, is_protein_CA ) );
		cur_job->add_string_real_pair( "startRMSbb",
			rmsd_with_super( native_pose, start_pose, is_protein_backbone ) );
		cur_job->add_string_real_pair( "startRMSall",
			rmsd_with_super(native_pose, start_pose, is_polymer_heavyatom ) );

		// final pose statistics - all peptide residues:
		cur_job->add_string_real_pair( "rmsCA",
			rmsd_with_super( native_pose, final_pose, is_protein_CA ) );
		cur_job->add_string_real_pair( "rmsBB",
			rmsd_with_super( native_pose, final_pose, is_protein_backbone ) );
		cur_job->add_string_real_pair( "rmsALL",
			rmsd_no_super_subset( native_pose, final_pose, superpos_partner, is_polymer_heavyatom ) );
	}

	if ( flags_.score_only ) {
		if ( !flags_.pep_fold_only ) {
			//fraction of native contacts
			cur_job->add_string_real_pair( "fnat5",
				fpdock_metrics_.calc_frac_native_contacts(native_pose, final_pose, 5.0 /*RMS threashold*/) );
			cur_job->add_string_real_pair( "fnat3.5",
				fpdock_metrics_.calc_frac_native_contacts(native_pose, final_pose, 3.5 /*RMS threashold*/) );
		}
		// torsion RMS data
		cur_job->add_string_real_pair( "startRMSphipsi",
			fpdock_metrics_.calc_phipsi_RMSD( native_pose, start_pose, superpos_partner) );
		cur_job->add_string_real_pair( "startRMSphipsi_if",
			fpdock_metrics_.calc_phipsi_RMSD( native_pose, start_pose, native_interface_residues) );
		cur_job->add_string_real_pair( "rmsPHIPSI",
			fpdock_metrics_.calc_phipsi_RMSD( native_pose, final_pose, superpos_partner) );
		cur_job->add_string_real_pair( "rmsPHIPSI_if",
			fpdock_metrics_.calc_phipsi_RMSD( native_pose, final_pose, native_interface_residues) );

		core::Size ngoodatoms;
		double frac1 =  fpdock_metrics_.calc_frac_atoms_kA_to_native
			( native_pose,
			final_pose,
			native_interface_residues,
			&is_polymer_heavyatom, 1 /*Angstrom*/, ngoodatoms);
		double frac1_bb =  fpdock_metrics_.calc_frac_atoms_kA_to_native
			( native_pose,
			final_pose,
			native_interface_residues,
			&is_protein_backbone, 1 /*Angstrom*/, ngoodatoms);
		double frac1_5 =  fpdock_metrics_.calc_frac_atoms_kA_to_native
			( native_pose,
			final_pose,
			native_interface_residues,
			&is_polymer_heavyatom, 1.5 /*Angstrom*/, ngoodatoms);
		double frac2 =  fpdock_metrics_.calc_frac_atoms_kA_to_native
			( native_pose,
			final_pose,
			native_interface_residues,
			&is_polymer_heavyatom, 2 /*Angstrom*/, ngoodatoms);
		cur_job->add_string_real_pair( "frac_iatoms_less_1A", frac1 );
		cur_job->add_string_real_pair( "frac_iatoms_less_1A_bb", frac1_bb );
		cur_job->add_string_real_pair( "frac_iatoms_less_1.5A", frac1_5 );
		cur_job->add_string_real_pair( "frac_iatoms_less_2A", frac2 );
		std::map<core::Size,core::Real> subsetRMS;
		for ( int k=4; k <= flags_.peptide_nres(); k++ ) {
			subsetRMS[k] = fpdock_metrics_.best_Kmer_rms
				( native_pose, final_pose,&is_polymer_heavyatom, k);
			std::ostringstream oss;
			oss << "bestRMS_" << k << "mer_all";
			std::string s = oss.str();
			cur_job->add_string_real_pair( s, subsetRMS[k] );
		}
	}

	// low-res stats:
	if ( flags_.lowres_preoptimize || flags_.lowres_abinitio ) {
		addLowResStatistics(start_pose, pose_after_lowres);
	}
	if ( flags_.pep_fold_only ) {
		addLowResStatistics(start_pose, pose_after_lowres);
	}

}

void FlexPepDockingProtocol::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const &,
	protocols::moves::Movers_map const &,
	core::pose::Pose const &
)
{
	scorefxn_ = protocols::rosetta_scripts::parse_score_function( tag, data );

	if ( tag->hasOption( "scorefxn_lowres") ) {
		std::string const scorefxn_lowres_key( tag->getOption<std::string>("scorefxn_lowres" ) );
		if ( ! data.has( "scorefxns", scorefxn_lowres_key ) ) {
			throw utility::excn::EXCN_RosettaScriptsOption("ScoreFunction " + scorefxn_lowres_key + " not found in basic::datacache::DataMap.");
		}
		scorefxn_lowres_ = data.get_ptr<core::scoring::ScoreFunction>( "scorefxns", scorefxn_lowres_key );
	}

	flags_.lowres_abinitio = tag->getOption<bool>( "lowres_abinitio", flags_.lowres_abinitio ) ;
	flags_.lowres_preoptimize = tag->getOption<bool>( "lowres_preoptimize", flags_.lowres_preoptimize ) ;
	flags_.min_only = tag->getOption<bool>( "min_only", flags_.min_only ) ;
	flags_.random_phi_psi_pert = tag->getOption<bool>( "random_phi_psi_pert", flags_.random_phi_psi_pert ) ;
	flags_.random_phi_psi_pert_size  = tag->getOption<core::Size>( "random_phi_psi_pert_size", (Size)(flags_.random_phi_psi_pert_size) ) ;
	flags_.extend = tag->getOption<bool>( "extend", flags_.extend ) ;
	flags_.place_peptide = tag->getOption<bool>( "place_peptide", flags_.place_peptide ) ;
	flags_.sample_pc = tag->getOption<core::Size>( "sample_pc", flags_.sample_pc ) ;
	flags_.slideintocontact = tag->getOption<bool>( "slidentocontact", flags_.slideintocontact ) ;
	flags_.rb_trans_size = tag->getOption<float>( "rb_trans_size", flags_.rb_trans_size) ;
	flags_.recal_foldtree = tag->getOption<bool>( "recal_foldtree", flags_.recal_foldtree ) ;
	flags_.rb_rot_size = tag->getOption<float>( "rb_rot_size", flags_.rb_rot_size) ;
	if ( tag->hasOption( "rb_trans_size" ) || tag->hasOption( "rb_rot_size" ) ) {
		flags_.randomRBstart = true;
	}
	flags_.pep_refine = tag->getOption<bool>( "pep_refine", flags_.pep_refine);
	if ( flags_.pep_refine || flags_.lowres_abinitio || flags_.lowres_preoptimize ) {
		// overrides obsolete rbMCM and torsionsMCM
		flags_.rbMCM = true;
		flags_.torsionsMCM = true;
	}
	flags_.peptide_loop_model = tag->getOption<bool>( "peptide_loop_model", flags_.peptide_loop_model) ;
	flags_.smove_angle_range =tag->getOption<float> ( "smove_angle_range", flags_.smove_angle_range) ;
	flags_.design_peptide = tag->getOption<bool>( "design_peptide", flags_.design_peptide) ;
	flags_.backrub_opt = tag->getOption<bool>( "backrub_opt", flags_.backrub_opt) ;
	flags_.boost_fa_atr = tag->getOption<bool>( "boost_fa_atr", flags_.boost_fa_atr) ;
	flags_.ramp_fa_rep = tag->getOption<bool>( "ramp_fa_rep", flags_.ramp_fa_rep) ;
	flags_.ramp_rama = tag->getOption<bool>( "ramp_rama", flags_.ramp_rama) ;
	flags_.rep_ramp_cycles =
		tag->getOption<core::Size>( "rep_ramp_cycles", flags_.rep_ramp_cycles) ;
	flags_.mcm_cycles = tag->getOption<core::Size>( "mcm_cycles", flags_.mcm_cycles) ;
	flags_.score_only = tag->getOption<bool>( "extra_scoring", flags_.score_only) ;
	flags_.use_cen_score = tag->getOption<bool>( "use_cen_score", flags_.use_cen_score) ;
	flags_.min_receptor_bb = tag->getOption<bool>( "min_receptor_bb", flags_.min_receptor_bb) ;
	flags_.ppk_only = tag->getOption<bool>( "ppk_only", flags_.ppk_only) ;
	flags_.no_prepack1 = tag->getOption<bool>( "no_prepack1", flags_.no_prepack1) ;
	flags_.no_prepack2 = tag->getOption<bool>( "no_prepack2", flags_.no_prepack2) ;
	flags_.score_filter = tag->getOption<bool>( "score_filter", flags_.score_filter) ;
	flags_.hb_filter  = tag->getOption<bool>( "hb_filter", flags_.hb_filter) ;
	flags_.hotspot_filter = tag->getOption<bool>( "hotspot_filter", flags_.hotspot_filter) ;
	flags_.frag3_weight  = tag->getOption<float>( "frag3_weight", flags_.frag3_weight) ;
	flags_.frag5_weight  = tag->getOption<float>( "frag5_weight", flags_.frag5_weight) ;
	flags_.frag9_weight  = tag->getOption<float>( "frag9_weight", flags_.frag9_weight) ;
	// todo: handle frag files themselves - this is right now in FlexPepDocingAbinitio.cc
	flags_.pSer2Asp_centroid = tag->getOption<bool>( "pSer2Asp_centroid", flags_.pSer2Asp_centroid) ;
	flags_.pSer2Glu_centroid = tag->getOption<bool>( "pSer2Glu_centroid", flags_.pSer2Glu_centroid) ;

	if ( tag->hasOption("receptor_chain") ) {
		flags_.set_receptor_chain
			( tag->getOption<std::string>( "receptor_chain" ) );
	}
	if ( tag->hasOption( "peptide_chain" ) ) {
		flags_.set_peptide_chain
			( tag->getOption<std::string>( "peptide_chain" ).at(0) );
	}

	// TODO: handle mutually exclusive options as in FlexPepDockingFlags

	// read native pose: (TODO: is this set automatically in scripts?)
	// protocols::jd2::set_native_in_mover(*fpDock);
	if ( tag->hasOption( "native" ) ) {
		core::pose::PoseOP tmp_native_pose( new core::pose::Pose );
		core::chemical::ResidueTypeSetCOP rsd_set;
		bool centroid_input = tag->getOption<bool>( "centroid_input", false );
		if ( centroid_input ) {
			core::import_pose::centroid_pose_from_pdb( *tmp_native_pose, tag->getOption<std::string>("native") , core::import_pose::PDB_file);
		} else {
			core::import_pose::pose_from_file( *tmp_native_pose, tag->getOption<std::string>("native") , core::import_pose::PDB_file);
		}
		this->set_native_pose( tmp_native_pose );
	}
}

//////////////////////////FlexPepDockingProtocolCreator//////////////////////////////

moves::MoverOP FlexPepDockingProtocolCreator::create_mover() const {
	return moves::MoverOP( new FlexPepDockingProtocol(1, true, true) );
}

std::string FlexPepDockingProtocolCreator::keyname() const {
	return FlexPepDockingProtocolCreator::mover_name();
}

std::string FlexPepDockingProtocolCreator::mover_name(){
	return "FlexPepDock";
}

} // end namespace flexpep_docking
} // end namespace protocols
