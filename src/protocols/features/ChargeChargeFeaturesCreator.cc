// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/feature/ChargeChargeFeaturesCreator.hh
/// @brief  Header for ChargeChargeFeaturesCreator for the ChargeChargeFeatures load-time factory registration scheme
/// @author Joe Harrison

// Unit Headers
#include <protocols/features/ChargeChargeFeaturesCreator.hh>

// Package Headers
#include <protocols/features/FeaturesReporterCreator.hh>
#include <protocols/features/ChargeChargeFeatures.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace features {

ChargeChargeFeaturesCreator::ChargeChargeFeaturesCreator() {}
ChargeChargeFeaturesCreator::~ChargeChargeFeaturesCreator() {}
FeaturesReporterOP ChargeChargeFeaturesCreator::create_features_reporter() const {
	return FeaturesReporterOP( new ChargeChargeFeatures );
}

std::string ChargeChargeFeaturesCreator::type_name() const {
	return "ChargeChargeFeatures";
}

} //namespace features
} //namespace protocols
