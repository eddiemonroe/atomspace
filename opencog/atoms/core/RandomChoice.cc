/*
 * RandomChoice.cc
 *
 * Copyright (C) 2015 Linas Vepstas
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

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/mt19937ar.h>

#include "../NumberNode.h"
#include "FunctionLink.h"
#include "RandomChoice.h"

using namespace opencog;

static MT19937RandGen randy(43);

RandomChoiceLink::RandomChoiceLink(const HandleSeq& oset,
                       TruthValuePtr tv, AttentionValuePtr av)
	: FunctionLink(RANDOM_CHOICE_LINK, oset, tv, av)
{
}

RandomChoiceLink::RandomChoiceLink(Link &l)
	: FunctionLink(l)
{
	// Type must be as expected
	Type tscope = l.getType();
	if (not classserver().isA(tscope, RANDOM_CHOICE_LINK))
	{
		const std::string& tname = classserver().getTypeName(tscope);
		throw InvalidParamException(TRACE_INFO,
			"Expecting an RandomChoiceLink, got %s", tname.c_str());
	}
}

// ---------------------------------------------------------------

Handle RandomChoiceLink::execute(AtomSpace * as) const
{
	size_t ary = _outgoing.size();
	if (0 == ary) return Handle();

	// Special-case handling for SetLinks, so it works with
	// dynamically-evaluated PutLinks ...
	Type ot = _outgoing[0]->getType();
	if (1 == ary and (SET_LINK == ot or LIST_LINK == ot))
	{
#if 0
		// Already executed by the time we get here... !?
		// XXX FIXME: this is a situation where doing "eager" execution
		// is a bad idea.
		FunctionLinkPtr flp(FunctionLinkCast(_outgoing[0]));
		if (nullptr == flp) return _outgoing[0];

		Handle h(flp->execute(as));
		LinkPtr lll(LinkCast(h));
		if (nullptr == lll) return h;
#endif

		LinkPtr lll(LinkCast(_outgoing[0]));
		ary = lll->getArity();
		return lll->getOutgoingAtom(randy.randint(ary));
	}

	if (2 == ary and (SET_LINK == ot or LIST_LINK == ot))
	{
		LinkPtr lweights(LinkCast(_outgoing[0]));
		LinkPtr lchoices(LinkCast(_outgoing[1]));

		if (lweights->getArity() != lchoices->getArity())
			throw SyntaxException(TRACE_INFO,
				"Weights and choices must be the same size");

		// Weights need to be numbers, or must evaluate to numbers.
		std::vector<double> weights;
		for (Handle h : lweights->getOutgoingSet())
		{
			FunctionLinkPtr flp(FunctionLinkCast(h));
			if (nullptr != flp)
				h = flp->execute(as);

			NumberNodePtr nn(NumberNodeCast(h));
			if (nullptr == nn)
				throw SyntaxException(TRACE_INFO,
				       "Expecting a NumberNode");
			weights.push_back(nn->get_value());
		}

		return lchoices->getOutgoingAtom(randy.randDiscrete(weights));
	}

	return _outgoing.at(randy.randint(ary));
}

/* ===================== END OF FILE ===================== */
