/*
 * UREConfigReader.h
 *
 * Copyright (C) 2015 OpenCog Foundation
 *
 * Author: Nil Geisweiller <ngeiswei@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _URE_CONFIG_READER_H
#define _URE_CONFIG_READER_H

#include "Rule.h"

#include <opencog/atomspace/AtomSpace.h>

namespace opencog {

/**
 * Read the URE configuration in the AtomSpace as described in
 * http://wiki.opencog.org/w/URE_Configuration_Format, and provide
 * parameter accessors for all rule-based systems.
 *
 * @todo: It doesn't support the hierarchical configuration structure
 * described in
 * http://wiki.opencog.org/w/URE_Configuration_Format#Rule-Based_System_Hierarchical_Structure,
 * instead it assumes all parameters are duplicated for all systems
 * and subsystems, for now.
 */
class UREConfigReader
{
public:
	// Ctor
	// rbs is a Handle pointing to a rule-based system is as
	UREConfigReader(AtomSpace& as, Handle rbs);

	// Access methods, return parameters given a rule-based system
	const std::vector<Rule>& get_rules() const;
	std::vector<Rule>& get_rules();
	bool get_attention_allocation() const;
	int get_maximum_iterations() const;

	// Name of the top rule base from which all rule-based systems
	// inherit. It should corresponds to a ConceptNode in the
	// AtomSpace.
	static const std::string URE_top_name;

	// Name of the PredicateNode outputing whether attention
	// allocation is enabled or not
	static const std::string attention_alloc_name;

	// Name of the SchemaNode outputing the maximum iterations
	// parameter
	static const std::string max_iter_name;
private:

	// Fetch from the AtomSpace all rule names of a given rube-based
	// system (i.e. rules members of that system).
	HandleSeq fetch_rules(Handle rbs);

	// Given <label> in
	//
	// EquivalenceLink
	//    <label>
	//    <body>
	//
	// Return <body>
	//
	// Normally this would be using DefineLink, but till it is not
	// fully supported we're using EquivalenceLink instead
	//
	// TODO: this could be moved in a utility library
	Handle fetch_definition(Handle label);

	// Given <schema> and <input> in
	//
	// ExecutionLink
	//    <schema>
	//    <input>
	//    <output>
	//
	// Return <output>
	//
	// TODO: this could be moved in a utility library
	Handle fetch_execution_output(Handle schema, Handle input);

	// Similar to above but takes instead the schema name instead of
	// the schema Handle, assumes that output value is a NumberNode,
	// and return directly that number.
	//
	// That is, given <schema_name> and <input> in
	//
	// ExecutionLink
	//    SchemaNode <schema_name>
	//    <input>
	//    NumberNode <num>
	//
	// Return the number associated to <num>
	double fetch_num_param(const std::string& schema_name, Handle input);

	// Given <pred_name> and <input> in
	//
	// EvaluationLink TV
	//    PredicateNode <pred_name>
	//    <input>
	//
	// Return TV.mean > 0.5
	bool fetch_bool_param(const std::string& pred_name, Handle input);
	
	AtomSpace& _as;

	class RuleBaseParameters {
	public:
		std::vector<Rule> rules;
		bool attention_alloc;
		int max_iter;
	};
	RuleBaseParameters _rbparams;
};

} // ~namespace opencog


#endif /* _URE_CONFIG_READER_H_ */
