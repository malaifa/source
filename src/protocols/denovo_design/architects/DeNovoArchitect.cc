// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file  protocols/denovo_design/architects/DeNovoArchitect.cc
/// @brief Designs topologies
/// @author Tom Linsky (tlinsky@uw.edu)

// Unit headers
#include <protocols/denovo_design/architects/DeNovoArchitect.hh>

// Protocol headers
#include <protocols/denovo_design/components/Segment.hh>
#include <protocols/denovo_design/components/StructureData.hh>
#include <protocols/denovo_design/util.hh>

// Basic/Utility headers
#include <basic/Tracer.hh>
#include <numeric/random/random.hh>
#include <utility/string_util.hh>
#include <utility/tag/Tag.hh>

static THREAD_LOCAL basic::Tracer TR( "protocols.denovo_design.architects.DeNovoArchitect" );

namespace protocols {
namespace denovo_design {
namespace architects {

DeNovoArchitect::DeNovoArchitect( std::string const & id ):
	StructureArchitect( id )
{}

DeNovoArchitect::~DeNovoArchitect()
{}

components::StructureDataOP
DeNovoArchitect::apply( core::pose::Pose const & pose ) const
{
	core::Real random = numeric::random::rg().uniform();
	return design( pose, random );
}

/////////////////////////////////////////////////////////////////////////////////////
DeNovoMotifArchitect::DeNovoMotifArchitect( std::string const & id ):
	DeNovoArchitect( id ),
	motifs_()
{}

DeNovoMotifArchitect::~DeNovoMotifArchitect()
{}

DeNovoArchitectOP
DeNovoMotifArchitect::clone() const
{
	return DeNovoArchitectOP( new DeNovoMotifArchitect( *this ) );
}

std::string
DeNovoMotifArchitect::type() const
{
	return architect_name();
}

components::StructureDataOP
DeNovoMotifArchitect::design( core::pose::Pose const &, core::Real & random ) const
{
	if ( motifs_.empty() ) {
		utility_exit_with_message( type() + "Architect requires one or more motifs or lengths to be specified" );
	}

	core::Size const idx = extract_int( random, 1, motifs_.size() );
	debug_assert( motifs_[ idx ] );
	Motif const & selected = *( motifs_[ idx ] );
	TR << id() << ": Selected motif " << selected
		<< "( " << idx << " of " << motifs_.size() << " )" << std::endl;
	components::StructureDataOP sd( new components::StructureData( id() ) );
	sd->add_segment( id(), selected );
	return sd;
}

void
DeNovoMotifArchitect::parse_tag( utility::tag::TagCOP tag, basic::datacache::DataMap & )
{
	std::string const motifs_str = tag->getOption< std::string >( "motif", "" );
	if ( !motifs_str.empty() ) set_motifs( motifs_str );
}


DeNovoMotifArchitect::MotifCOPs::const_iterator
DeNovoMotifArchitect::motifs_begin() const
{
	return motifs_.begin();
}

DeNovoMotifArchitect::MotifCOPs::const_iterator
DeNovoMotifArchitect::motifs_end() const
{
	return motifs_.end();
}

void
DeNovoMotifArchitect::set_motifs( std::string const & motifs_str )
{
	MotifCOPs motif_vec;
	utility::vector1< std::string > const motif_strs = utility::string_split( motifs_str, ',' );
	for ( utility::vector1< std::string >::const_iterator ms=motif_strs.begin(); ms!=motif_strs.end(); ++ms ) {
		MotifOP newmotif( new Motif() );
		newmotif->parse_motif( *ms );
		motif_vec.push_back( newmotif );
	}
	set_motifs( motif_vec );
}

void
DeNovoMotifArchitect::set_motifs( MotifCOPs const & motifs )
{
	motifs_ = motifs;
}

/////////////////////////////////////////////////////////////////////////////////////

SecStructInfo
generate_secstruct_for_length(
	char const ss_char,
	std::string const & abego,
	core::Size const len )
{
	SecStructInfo ss_abego;
	ss_abego.ss += 'L';
	ss_abego.abego.push_back( "X" );
	for ( core::Size i=1; i<len; ++i ) {
		ss_abego.ss += ss_char;
		ss_abego.abego.push_back( abego );
	}
	ss_abego.ss += 'L';
	ss_abego.abego.push_back( "X" );
	return ss_abego;
}

} //protocols
} //denovo_design
} //architects
