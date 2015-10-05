/*
 * VariableList.cc
 *
 * Copyright (C) 2009, 2014, 2015 Linas Vepstas
 *
 * Author: Linas Vepstas <linasvepstas@gmail.com>  January 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the
 * exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <opencog/atomspace/ClassServer.h>
#include <opencog/atoms/TypeNode.h>

#include "VariableList.h"

using namespace opencog;

void VariableList::validate_vardecl(const HandleSeq& oset)
{
	for (const Handle& h: oset)
	{
		Type t = h->getType();
		if (VARIABLE_NODE == t)
		{
			_varlist.varset.insert(h);    // tree (unordered)
			_varlist.varseq.emplace_back(h); // vector (ordered)
		}
		else if (TYPED_VARIABLE_LINK == t)
		{
			get_vartype(h);
		}
		else
			throw InvalidParamException(TRACE_INFO,
				"Expected a VariableNode or a TypedVariableLink, got: %s",
					classserver().getTypeName(t).c_str());
	}
	build_index();
}

VariableList::VariableList(const HandleSeq& oset,
                       TruthValuePtr tv, AttentionValuePtr av)
	: Link(VARIABLE_LIST, oset, tv, av)
{
	validate_vardecl(oset);
}

VariableList::VariableList(Type t, const HandleSeq& oset,
                       TruthValuePtr tv, AttentionValuePtr av)
	: Link(t, oset, tv, av)
{
	// derived classes have a different initialization order
	if (VARIABLE_LIST != t) return;
	validate_vardecl(oset);
}

VariableList::VariableList(Link &l)
	: Link(l)
{
	// Type must be as expected
	Type tscope = l.getType();
	if (not classserver().isA(tscope, VARIABLE_LIST))
	{
		const std::string& tname = classserver().getTypeName(tscope);
		throw InvalidParamException(TRACE_INFO,
			"Expecting a VariableList, got %s", tname.c_str());
	}

	// Dervided types have a different initialization sequence
	if (VARIABLE_LIST != tscope) return;
	validate_vardecl(_outgoing);
}

/* ================================================================= */
typedef std::pair<Handle, const std::set<Type> > ATPair;

/**
 * Extract the variable type(s) from a TypedVariableLink
 *
 * The call is expecting htypelink to point to one of the two
 * following structures:
 *
 *    TypedVariableLink
 *       VariableNode "$some_var_name"
 *       TypeNode  "ConceptNode"
 *
 * or
 *
 *    TypedVariableLink
 *       VariableNode "$some_var_name"
 *       TypeChoice
 *          TypeNode  "ConceptNode"
 *          TypeNode  "NumberNode"
 *          TypeNode  "WordNode"
 *
 * In either case, the variable itself is appended to "vset",
 * and the list of allowed types are associated with the variable
 * via the map "typemap".
 */
void VariableList::get_vartype(const Handle& htypelink)
{
	const std::vector<Handle>& oset = LinkCast(htypelink)->getOutgoingSet();
	if (2 != oset.size())
	{
		throw InvalidParamException(TRACE_INFO,
			"TypedVariableLink has wrong size, got %lu", oset.size());
	}

	Handle varname = oset[0];
	Handle vartype = oset[1];

	// The vartype is either a single type name, or a list of typenames.
	Type t = vartype->getType();
	if (TYPE_NODE == t)
	{
		Type vt = TypeNodeCast(vartype)->get_value();
		std::set<Type> ts = {vt};
		_varlist.typemap.insert(ATPair(varname, ts));
		_varlist.varset.insert(varname);
		_varlist.varseq.emplace_back(varname);
	}
	else if (TYPE_CHOICE == t)
	{
		std::set<Type> ts;

		const std::vector<Handle>& tset = LinkCast(vartype)->getOutgoingSet();
		size_t tss = tset.size();
		for (size_t i=0; i<tss; i++)
		{
			Handle h(tset[i]);
			Type var_type = h->getType();
			if (TYPE_NODE != var_type)
			{
				throw InvalidParamException(TRACE_INFO,
					"VariableChoice has unexpected content:\n"
				              "Expected TypeNode, got %s",
				              classserver().getTypeName(h->getType()).c_str());
			}
			Type vt = TypeNodeCast(h)->get_value();
			ts.insert(vt);
		}

		_varlist.typemap.insert(ATPair(varname,ts));
		_varlist.varset.insert(varname);
		_varlist.varseq.emplace_back(varname);
	}
	else
	{
		throw InvalidParamException(TRACE_INFO,
			"Unexpected contents in TypedVariableLink\n"
			"Expected TypeNode or TypeChoice, got %s",
			classserver().getTypeName(t).c_str());
	}
}

/* ================================================================= */
/**
 * Validate variable declarations for syntax correctness.
 *
 * This will check to make sure that a set of variable declarations are
 * of a reasonable form. Thus, for example, a structure similar to the
 * below is expected.
 *
 *       VariableList
 *          VariableNode "$var0"
 *          VariableNode "$var1"
 *          TypedVariableLink
 *             VariableNode "$var2"
 *             TypeNode  "ConceptNode"
 *          TypedVariableLink
 *             VariableNode "$var3"
 *             TypeChoice
 *                 TypeNode  "PredicateNode"
 *                 TypeNode  "GroundedPredicateNode"
 *
 * As a side-effect, the variables and type restrictions are unpacked.
 */
void VariableList::validate_vardecl(const Handle& hdecls)
{
	// Expecting the declaration list to be either a single
	// variable, or a list of variable declarations
	Type tdecls = hdecls->getType();
	if ((VARIABLE_NODE == tdecls) or
	    NodeCast(hdecls)) // allow *any* node as a variable
	{
		_varlist.varset.insert(hdecls);
		_varlist.varseq.emplace_back(hdecls);
	}
	else if (TYPED_VARIABLE_LINK == tdecls)
	{
		get_vartype(hdecls);
	}
	else if (VARIABLE_LIST == tdecls or LIST_LINK == tdecls)
	{
		// The list of variable declarations should be .. a list of
		// variables! Make sure its as expected.
		const std::vector<Handle>& dset = LinkCast(hdecls)->getOutgoingSet();
		validate_vardecl(dset);
	}
	else
	{
		throw InvalidParamException(TRACE_INFO,
			"Expected a VariableList holding variable declarations");
	}
	build_index();
}

/* ================================================================= */
/**
 * Build the index from variable name, to its ordinal number.
 * The index is needed for variable substitution, i.e. for the
 * substitute method below.  The specific sequence order of the
 * variables is essential for making substitution work.
 */
void VariableList::build_index(void)
{
	if (0 < _varlist.index.size()) return;
	size_t sz = _varlist.varseq.size();
	for (size_t i=0; i<sz; i++)
	{
		_varlist.index.insert(std::pair<Handle, unsigned int>(_varlist.varseq[i], i));
	}
}

/* ================================================================= */
/**
 * Simple type checker.
 *
 * Returns true/false if the indicated handle is of the type that
 * we have memoized.  If this typelist contians more than one type in
 * it, then clearly, there is a mismatch.  If there are no type
 * restrictions, then it is trivially a match.  Otherwise, there must
 * be a TypeChoice, and so the handle must be one of the types in the
 * TypeChoice.
 */
bool Variables::is_type(const Handle& h) const
{
	// The arity must be one for there to be a match.
	if (1 != varset.size()) return false;

	// No type restrictions.
	if (0 == typemap.size()) return true;

	// Check the type restrictions.
	VariableTypeMap::const_iterator it =
		typemap.find(varseq[0]);
	const std::set<Type> &tchoice = it->second;

	Type htype = h->getType();
	std::set<Type>::const_iterator allow = tchoice.find(htype);
	return allow != tchoice.end();
}

/* ================================================================= */
/**
 * Very simple type checker.
 *
 * Returns true/false if the indicated handles are of the type that
 * we have memoized.
 *
 * XXX TODO this does not currently handle type equations, as outlined
 * on the wiki; We would need the general pattern matcher to do type
 * checking, in that situation.
 */
bool Variables::is_type(const HandleSeq& hseq) const
{
	// The arity must be one for there to be a match.
	size_t len = hseq.size();
	if (varset.size() != len) return false;
	// No type restrictions.
	if (0 == typemap.size()) return true;

	// Check the type restrictions.
	for (size_t i=0; i<len; i++)
	{
		VariableTypeMap::const_iterator it =
			typemap.find(varseq[i]);
		if (it == typemap.end()) continue;  // no restriction

		const std::set<Type> &tchoice = it->second;
		Type htype = hseq[i]->getType();
		std::set<Type>::const_iterator allow = tchoice.find(htype);
		if (allow == tchoice.end()) return false;
	}
	return true;
}

/* ================================================================= */
/**
 * Substitute variables in tree with the indicated values.
 * This is a lot like applying the function fun to the argument list
 * args, except that no actual evaluation is performed; only
 * substitution.  The resulting tree is NOT placed into any atomspace,
 * either. If you want that, you must do it yourself.  If you want
 * evaluation or execution to happen during sustitution, use either
 * the EvaluationLink, the ExecutionOutputLink, or the Instantiator.
 *
 * So, for example, if this VariableList contains:
 *
 *   VariableList
 *       VariableNode $a
 *       VariableNode $b
 *
 * and func is the template:
 *
 *   EvaluationLink
 *      PredicateNode "something"
 *      ListLink
 *         VariableNode $b      ; note the reversed order
 *         VariableNode $a
 *
 * and the args is a list
 *
 *      ConceptNode "one"
 *      NumberNode 2.0000
 *
 * then the returned value will be
 *
 *   EvaluationLink
 *      PredicateNode "something"
 *      ListLink
 *          NumberNode 2.0000    ; note reversed order here, also
 *          ConceptNode "one"
 *
 * That is, the values "one" and 2.0 were substituted for $a and $b.
 *
 * The func can be, for example, a single variable name(!) In this
 * case, the corresponding arg is returned. So, for example, if the
 * func was simple $b, then 2.0 would be returned.
 *
 * Type checking is performed before subsitution; if the args fail to
 * satisfy the type constraints, an exception is thrown.
 *
 * Again, only a substitution is performed, there is no evaluation.
 * Note also that the resulting tree is NOT placed into any atomspace!
 */
Handle Variables::substitute(const Handle& fun,
                             const HandleSeq& args) const
{
	if (args.size() != varseq.size())
		throw InvalidParamException(TRACE_INFO,
			"Incorrect number of arguments specified, expecting %lu got %lu",
			varseq.size(), args.size());

	if (not is_type(args))
		throw InvalidParamException(TRACE_INFO,
			"Arguments fail to match variable declarations");

	return substitute_nocheck(fun, args);
}

Handle Variables::substitute_nocheck(const Handle& term,
                                     const HandleSeq& args) const
{
	// If it is a singleton, just return that singleton.
	std::map<Handle, unsigned int>::const_iterator idx;
	idx = index.find(term);
	if (idx != index.end())
		return args.at(idx->second);

	// If its a node, and its not a variable, then it is a constant,
	// and just return that.
	LinkPtr lterm(LinkCast(term));
	if (NULL == lterm) return term;

	// QuoteLinks halt the reursion
	// XXX TODO -- support UnquoteLink
	if (QUOTE_LINK == term->getType()) return term;

	// Recursively fill out the subtrees.
	HandleSeq oset;
	for (const Handle& h : lterm->getOutgoingSet())
	{
		oset.emplace_back(substitute_nocheck(h, args));
	}
	return Handle(createLink(term->getType(), oset));
}

/* ================================================================= */
/**
 * Extend a set of variables.
 *
 */
void Variables::extend(const Variables& vset)
{
	for (const Handle& h : vset.varseq)
	{
		try
		{
			index.at(h);

			// Merge the two typemaps, if needed.
			try
			{
				const std::set<Type>& tms = vset.typemap.at(h);
				std::set<Type> mytypes = typemap[h];
				for (Type t : tms)
					mytypes.insert(t);
				typemap.erase(h);	 // is it safe to erase if h not in already?
				typemap.insert({h,mytypes});
			}
			catch(const std::out_of_range&) {}
		}
		catch(const std::out_of_range&)
		{
			// Found a new variable! Insert it.
			index.insert({h, varseq.size()});
			varseq.emplace_back(h);
			varset.insert(h);

			// Install the type constraints, as well.
			// The at() might throw...
			try
			{
				typemap.insert({h, vset.typemap.at(h)});
			}
			catch(const std::out_of_range&) {}
		}
	}
}

/* ===================== END OF FILE ===================== */
