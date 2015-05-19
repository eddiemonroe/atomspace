/*
 * InferenceSCM.cc
 *
 * Copyright (C) 2014 Misgana Bayetta
 * Copyright (C) 2015 OpenCog Foundation
 *
 * Author: Misgana Bayetta <misgana.bayetta@gmail.com>  Sept 2014
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

#include "InferenceSCM.h"

#include <opencog/guile/SchemePrimitive.h>
#include <opencog/guile/SchemeSmob.h>
#include <opencog/rule-engine/forwardchainer/ForwardChainer.h>
#include <opencog/rule-engine/forwardchainer/DefaultForwardChainerCB.h>
#include <opencog/rule-engine/backwardchainer/BackwardChainer.h>
#include <opencog/atomspace/AtomSpace.h>

#include <opencog/rule-engine/forwardchainer/EMForwardChainer.h>
#include <opencog/rule-engine/forwardchainer/EMForwardChainerCB.h>

using namespace opencog;

InferenceSCM::InferenceSCM()
{
    static bool is_init = false;
    if (is_init) return;
    is_init = true;
    scm_with_guile(init_in_guile, this);
}

/**
 * Init function for using with scm_with_guile.
 *
 * Creates the scheme module and uses it by default.
 *
 * @param self   pointer to the InferenceSCM object
 * @return       null
 */
void* InferenceSCM::init_in_guile(void* self)
{
    scm_c_define_module("opencog rule-engine", init_in_module, self);
    scm_c_use_module("opencog rule-engine");
    return NULL;
}

/**
 * The main function for defining stuff in the scheme module.
 *
 * @param data   pointer to the InferenceSCM object
 */
void InferenceSCM::init_in_module(void* data)
{
    InferenceSCM* self = (InferenceSCM*) data;
    self->init();
}


void InferenceSCM::init(void)
{
#ifdef HAVE_GUILE
    // All commands for invoking the rule engine from scm shell should
    // be declared here
    define_scheme_primitive("cog-fc", &InferenceSCM::do_forward_chaining,
                            this, "rule-engine");
    define_scheme_primitive("cog-bc", &InferenceSCM::do_backward_chaining,
                            this, "rule-engine");
    define_scheme_primitive("cog-fc-em",&InferenceSCM::do_forward_chaining_em,
                            this, "rule-engine");
    define_scheme_primitive("cog-fc-em2",&InferenceSCM::do_forward_chaining_em_default_control,
                            this, "rule-engine");
#endif
}

Handle InferenceSCM::do_forward_chaining(Handle h, const string& conf_path)
{
#ifdef HAVE_GUILE
    AtomSpace *as = SchemeSmob::ss_get_env_as("cog-fc");
    DefaultForwardChainerCB dfc(as);
    ForwardChainer fc(as, conf_path);
    /**
     * Parse (cog-fc ListLink()) as forward chaining with
     * Handle::UNDEFINED which does pattern matching on the atomspace
     * using the rules declared in the config. A similar functionality
     * with the python version of the forward chainer.
     */
    if (h->getType() == LIST_LINK and as->getOutgoing(h).empty())
        fc.do_chain(dfc, Handle::UNDEFINED);
    else
        /** Does variable fulfillment forward chaining or forward chaining based on
         *  target node @param h.
         *  example (cog-fc (InheritanceLink (VariableNode "$X") (ConceptNode "Human")))
         *  finds all the matches for $X by first finding matching rules and then applying
         *  all of them using the pattern matcher.
         *  and (cog-fc (ConceptNode "Human")) will start forward chaining on the concept Human
         *  trying to generate inferences associated only with the conceptNode Human.
         */
        fc.do_chain(dfc, h);

    HandleSeq result = fc.get_chaining_result();
    return as->addLink(LIST_LINK, result);
#else
    return Handle::UNDEFINED;
#endif
}

Handle InferenceSCM::do_forward_chaining_em_default_control(Handle source) {
    return do_forward_chaining_em(source,NULL);
}


Handle InferenceSCM::do_forward_chaining_em(Handle source, const string& conf_path)
{
#ifdef HAVE_GUILE
    //printf("InferenceSCM::do_forward_chaining_em\n");
    AtomSpace *as = SchemeSmob::ss_get_env_as("cog-fc-em");
    //printf("create forward chainer\n");
    EMForwardChainer fc(as);
    //printf("create fc callback\n");
    EMForwardChainerCB fccb(&fc);

    /**
     * Parse (cog-fc ListLink()) as forward chaining with Handle::UNDEFINED which  does
     * pattern matching on the atomspace using the rules declared in the config.A similar
     * functionality with the python version of the  forward chainer.
     */
    // ??? so if the source is a listlink and there's nothing pointing to it, then don't use it?
    //re and as->getIncoming(h).empty(): hmmmm.. what if list happens to be used somewhere else in the atomspace
//    if (h->getType() == LIST_LINK and as->getIncoming(h).empty())
//        fc.do_chain(fccb, Handle::UNDEFINED);
//    else
        /** Does variable fulfillment forward chaining or forward chaining based on
         *  target node @param h.
         *  example (cog-fc (InheritanceLink (VariableNode "$X") (ConceptNode "Human")))
         *  finds all the matches for $X by first finding matching rules and then applying
         *  all of them using the pattern matcher.
         *  and (cog-fc (ConceptNode "Human")) will start forward chaining on the concept Human
         *  trying to generate inferences associated only with the conceptNode Human.
         */
        fc.do_chain(source,&fccb);

    HandleSeq result = fc.get_chaining_result();
    return as->addLink(LIST_LINK, result);
#else
    return Handle::UNDEFINED;
#endif
}

Handle InferenceSCM::do_backward_chaining(Handle h)
{
#ifdef HAVE_GUILE
    AtomSpace *as = SchemeSmob::ss_get_env_as("cog-bc");

    JsonicControlPolicyParamLoader cpolicy_loader(JsonicControlPolicyParamLoader(as, "reasoning/default_cpolicy.json"));
    cpolicy_loader.load_config();

    std::vector<Rule> rules;
    for (Rule* pr : cpolicy_loader.get_rules())
        rules.push_back(*pr);

    BackwardChainer bc(as, rules);
	bc.set_target(h);

	logger().debug("[BackwardChainer] Before do_chain");

    bc.do_until(cpolicy_loader.get_max_iter());

	logger().debug("[BackwardChainer] After do_chain");
    map<Handle, UnorderedHandleSet> soln = bc.get_chaining_result();

    HandleSeq soln_list_link;
    for (auto it = soln.begin(); it != soln.end(); ++it) {
        HandleSeq hs;
        hs.push_back(it->first);
        hs.insert(hs.end(), it->second.begin(), it->second.end());

        soln_list_link.push_back(as->addLink(LIST_LINK, hs));
    }

    return as->addLink(LIST_LINK, soln_list_link);
#else
    return Handle::UNDEFINED;
#endif
}


void opencog_ruleengine_init(void)
{
    static InferenceSCM inference;
}
