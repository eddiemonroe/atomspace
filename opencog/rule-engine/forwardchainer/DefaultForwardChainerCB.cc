/*
 * DefaultForwardChainerCB.cc
 *
 * Copyright (C) 2015 Misgana Bayetta
 *
 * Author: Misgana Bayetta <misgana.bayetta@gmail.com>
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

#include <opencog/atomutils/AtomUtils.h>
#include <opencog/guile/SchemeSmob.h>
#include <opencog/atoms/bind/BindLink.h>
#include <opencog/query/DefaultImplicator.h>

#include "DefaultForwardChainerCB.h"
#include "../URECommons.h"

#include <opencog/util/Logger.h>

using namespace opencog;


DefaultForwardChainerCB::DefaultForwardChainerCB(AtomSpace& as,
                                                 source_selection_mode ts_mode
                                                 /*=TV_FITNESS_BASED*/)
	: ForwardChainerCallBack(&as), _as(as), _fcpm(&as), _ts_mode(ts_mode) {}

/**
 * choose rules based on premises of rule matching the source
 * uses temporary atomspace to limit the search space
 *
 * @param fcmem forward chainer's working memory
 * @return a vector of chosen rules
 */
vector<Rule*> DefaultForwardChainerCB::choose_rules(FCMemory& fcmem)
{
    Handle source = fcmem.get_cur_source();
    if (source == Handle::UNDEFINED)
        throw InvalidParamException(TRACE_INFO,
                                    "Needs a source atom of type LINK");
    HandleSeq chosen_bindlinks;

    if (LinkCast(source)) {
        AtomSpace rule_atomspace;
        Handle source_cpy = rule_atomspace.add_atom(source);

        // Copy rules to the temporary atomspace.
        vector<Rule*> rules = fcmem.get_rules();
        for (Rule* r : rules) {
            rule_atomspace.add_atom(r->get_handle());
        }

        _log.debug("All rules:");
        for (Rule* r: rules) {
            _log.debug(r->get_name());
        }

        // Create bindlink with source as an implicant.
        URECommons urec(rule_atomspace);
        Handle copy = urec.replace_nodes_with_varnode(source_cpy, NODE);
        Handle bind_link = urec.create_bindLink(copy, false);

        // Pattern match.
        BindLinkPtr bl(BindLinkCast(bind_link));
        DefaultImplicator imp(&rule_atomspace);
        imp.implicand = bl->get_implicand();
        bl->imply(imp);

        // Get matched bindLinks.
        HandleSeq matches = imp.result_list;
        if (matches.empty()) {
            _log.debug(
                    "No matching BindLink was found. Returning empty vector");
            return vector<Rule*> { };
        }

        UnorderedHandleSet bindlinks;
        for (Handle hm : matches) {
            //get all BindLinks whose part of their premise matches with hm
            HandleSeq hs = get_rootlinks(hm, &rule_atomspace, BIND_LINK);
            bindlinks.insert(hs.cbegin(), hs.cend());
        }

        // Copy handles to main atomspace.
        for (Handle h : bindlinks) {
            chosen_bindlinks.push_back(_as.add_atom(h));
        }
    } else {
        // Try to find specialized rules that contain the source node.
        _log.debug("source is not a link. Try to find specialized rules that contain the source node.");
        OC_ASSERT(NodeCast(source) != nullptr);
        chosen_bindlinks = get_rootlinks(source, &_as, BIND_LINK);
    }

    _log.debug("chosen_bindlinks:");
    //sleep(1);

    for (auto bl : chosen_bindlinks) {
        _log.debug(bl->toString());
        //sleep(1);

    }

    // Find the rules containing the bindLink in copied_back.
    vector<Rule*> matched_rules;
    for (Rule* r : fcmem.get_rules()) {
        auto it = find(chosen_bindlinks.begin(), chosen_bindlinks.end(),
                       r->get_handle()); //xxx not matching
        if (it != chosen_bindlinks.end()) {
            matched_rules.push_back(r);
        }
    }

    return matched_rules;
}

/**
 * Gets all top level links of certain types and subclasses that
 * contain @param hsource
 *
 * @param hsource handle whose top level links are to be searched
 * @param as the atomspace in which search is to be done
 * @param link_type the root link types to be searched
 * @param subclasses a flag that tells to look subclasses of @link_type
 */
HandleSeq DefaultForwardChainerCB::get_rootlinks(Handle hsource, AtomSpace* as,
                                                 Type link_type,
                                                 bool subclasses)
{
    URECommons urec(*as);
    HandleSeq chosen_roots;
    HandleSeq candidates_roots;
    urec.get_root_links(hsource, candidates_roots);

    for (Handle hr : candidates_roots) {
        bool notexist = find(chosen_roots.begin(), chosen_roots.end(), hr)
                == chosen_roots.end();
        auto type = as->get_type(hr);
        bool subtype = (subclasses and classserver().isA(type, link_type));
        if (((type == link_type) or subtype) and notexist) {
            // Make sure matches are actually part of the premise list
            // rather than the output of the bindLink
	        Handle hpremise = BindLinkCast(hr)->get_body();
            if (urec.exists_in(hpremise, hsource)) {
                chosen_roots.push_back(hr);
            }
        }

    }

    return chosen_roots;
}
HandleSeq DefaultForwardChainerCB::choose_premises(FCMemory& fcmem)
{
    HandleSeq inputs;
    URECommons urec(_as);
    Handle hsource = fcmem.get_cur_source();

    // Get everything associated with the source handle.
    UnorderedHandleSet neighbors = get_distant_neighbors(hsource, 2);

    // Add all root links of atoms in @param neighbors.
    //sleep(1);

    _log.debug("Neighbors (2 steps):");

    for (auto hn : neighbors) {
        if (hn->getType() != VARIABLE_NODE) {
            _log.fine(hn->toString());
            ////sleep(1);

            HandleSeq roots;
            urec.get_root_links(hn, roots);
            _log.debug("----------------- \nRoots:");
            ////sleep(1);

            for (auto r : roots) {
                _log.fine(r->toString("rootroot--> "));
                //sleep(1);
                if (find(inputs.begin(), inputs.end(), r) == inputs.end() and r->getType()
                        != BIND_LINK)
                    inputs.push_back(r);
            }
            _log.debug("-----------------");
        }
    }


    return inputs;
}

Handle DefaultForwardChainerCB::choose_next_source(FCMemory& fcmem)
{
    HandleSeq tlist = fcmem.get_potential_sources();
    map<Handle, float> tournament_elem;
    URECommons urec(_as);
    Handle hchosen = Handle::UNDEFINED;

    for (Handle t : tlist) {
        switch (_ts_mode) {
        case TV_FITNESS_BASED: {
            float fitness = urec.tv_fitness(t);
            tournament_elem[t] = fitness;
        }
            break;
        case STI_BASED:
            tournament_elem[t] = t->getSTI();
            break;
        default:
            throw RuntimeException(TRACE_INFO,
                                   "Unknown source selection mode.");
            break;
        }
    }

    //!Choose a new source that has never been chosen before.
    //!xxx FIXME since same handle might be chosen multiple times the following
    //!code doesn't guarantee all sources have been exhaustively looked.
    for (size_t i = 0; i < tournament_elem.size(); i++) {
        Handle hselected = urec.tournament_select(tournament_elem);
        if (fcmem.isin_selected_sources(hselected)) {
            continue;
        } else {
            hchosen = hselected;
            break;
        }
    }

    // Incase of when all sources are selected
    if (hchosen == Handle::UNDEFINED)
        return urec.tournament_select(tournament_elem);

    return hchosen;
}

HandleSeq DefaultForwardChainerCB::apply_rule(FCMemory& fcmem)
{
    _fcpm.set_fcmem(&fcmem);

    auto rule_handle = fcmem.get_cur_rule()->get_handle();
    BindLinkPtr bl(BindLinkCast(rule_handle));
    if (NULL == bl) {
        bl = createBindLink(*LinkCast(rule_handle));
    }
    _fcpm.implicand = bl->get_implicand();
    bl->imply(_fcpm);
    // bl->satisfy(*_fcpm);

    HandleSeq product = _fcpm.get_products();

    //! Make sure the inferences made are new.
    for (auto iter = product.begin(); iter != product.end();) {
        if (fcmem.isin_potential_sources(*iter))
            iter = product.erase(iter);
        else
            ++iter;
    }

    return product;
}

void DefaultForwardChainerCB::set_logger(Logger l) {
    _log = l;
}
