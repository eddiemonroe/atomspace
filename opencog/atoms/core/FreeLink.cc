/*
 * opencog/atoms/core/FreeLink.cc
 *
 * Copyright (C) 2015 Linas Vepstas
 * All Rights Reserved
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

#include <opencog/atomspace/atom_types.h>
#include <opencog/atomspace/ClassServer.h>
#include "FreeLink.h"

using namespace opencog;

FreeLink::FreeLink(const HandleSeq& oset,
                   TruthValuePtr tv,
                   AttentionValuePtr av)
    : Link(FREE_LINK, oset, tv, av)
{
	init();
}

FreeLink::FreeLink(const Handle& a,
                   TruthValuePtr tv,
                   AttentionValuePtr av)
    : Link(FREE_LINK, a, tv, av)
{
	init();
}

FreeLink::FreeLink(Type t, const HandleSeq& oset,
                   TruthValuePtr tv,
                   AttentionValuePtr av)
    : Link(t, oset, tv, av)
{
	if (not classserver().isA(t, FREE_LINK))
		throw InvalidParamException(TRACE_INFO, "Expecting a FreeLink");

	// Derived classes have thier own init routines.
	if (FREE_LINK != t) return;
	init();
}

FreeLink::FreeLink(Type t, const Handle& a,
                   TruthValuePtr tv,
                   AttentionValuePtr av)
    : Link(t, a, tv, av)
{
	if (not classserver().isA(t, FREE_LINK))
		throw InvalidParamException(TRACE_INFO, "Expecting a FreeLink");

	// Derived classes have thier own init routines.
	if (FREE_LINK != t) return;
	init();
}

FreeLink::FreeLink(Type t, const Handle& a, const Handle& b,
                   TruthValuePtr tv,
                   AttentionValuePtr av)
    : Link(t, a, b, tv, av)
{
	if (not classserver().isA(t, FREE_LINK))
		throw InvalidParamException(TRACE_INFO, "Expecting a FreeLink");

	// Derived classes have thier own init routines.
	if (FREE_LINK != t) return;
	init();
}

FreeLink::FreeLink(Link& l)
    : Link(l)
{
	Type tscope = l.getType();
	if (not classserver().isA(tscope, FREE_LINK))
		throw InvalidParamException(TRACE_INFO, "Expecting a FreeLink");

	// Derived classes have thier own init routines.
	if (FREE_LINK != tscope) return;
	init();
}

/* ================================================================= */
/// Create an ordered set of the free variables in this link.
///
/// By "ordered set" it is meant: a list of variables, in traversal
/// order (from left to right, as they appear in the tree), with each
/// variable being named only once.  The varset is only used to make
/// sure that we don't name a variable more than once; that's all.
void FreeLink::find_vars(std::set<Handle>& varset, const HandleSeq& oset)
{
	for (const Handle& h : oset)
	{
		Type t = h->getType();
		if (QUOTE_LINK == t) continue;
		if (VARIABLE_NODE == t and
		    0 == varset.count(h))
		{
			_varseq.push_back(h);
			varset.insert(h);
		}
		LinkPtr lll(LinkCast(h));
		if (NULL == lll) continue;

		find_vars(varset, lll->getOutgoingSet());
	}
}

/* ================================================================= */
/**
 * Build the index from variable name, to its ordinal number.
 * The index is needed for variable substitution, i.e. for the
 * reduce() method.  The specific sequence order of the variables
 * is essential for making substitution work.
 */
void FreeLink::build_index(void)
{
	if (0 < _index.size()) return;
	size_t sz = _varseq.size();
	for (size_t i=0; i<sz; i++)
		_index.insert(std::pair<Handle, unsigned int>(_varseq[i], i));
}

/* ================================================================= */

void FreeLink::init(void)
{
	std::set<Handle> varset;
	find_vars(varset, _outgoing);
	build_index();
}

Handle FreeLink::reduce(void)
{
   throw RuntimeException(TRACE_INFO, "Not reducible!");
}
