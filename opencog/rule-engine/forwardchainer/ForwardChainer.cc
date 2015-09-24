/*
 * ForwardChainer.cc
 *
 * Copyright (C) 2014,2015
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

#include <opencog/util/Logger.h>

#include <opencog/atoms/bind/BindLink.h>
#include <opencog/atoms/bind/PatternLink.h>
#include <opencog/atomutils/AtomUtils.h>
#include <opencog/atomutils/FindUtils.h>
#include <opencog/atomutils/Substitutor.h>
#include <opencog/query/BindLinkAPI.h>
#include <opencog/query/DefaultImplicator.h>
#include <opencog/rule-engine/Rule.h>

#include "ForwardChainer.h"
#include "FocusSetPMCB.h"
#include "VarGroundingPMCB.h"

using namespace opencog;

int MAX_PM_RESULTS = 10000;

ForwardChainer::ForwardChainer(AtomSpace& as, Handle rbs) :
        _as(as), _rec(as), _rbs(rbs), _configReader(as, rbs), _fcmem(&as)
{
    cout << "ForwardChainer::ForwardChainer()" << endl;
    init();
}

// temporary util until rule names are fixed
const char * getFormulaName(Rule *rule) {
    return NodeCast((LinkCast(rule->get_implicand()))->getOutgoingSet()[0])->getName().c_str();
}

void ForwardChainer::init()
{

    cout << "ForwardChainer::init()" << endl;

    _fcmem.set_search_in_af(_configReader.get_attention_allocation());
    _fcmem.set_rules(_configReader.get_rules());
    _fcmem.set_cur_rule(nullptr);
    
    _ts_mode = TV_FITNESS_BASED;

    // Provide a logger
    _log = NULL;
    setLogger(new opencog::Logger("forward_chainer.log", Logger::FINE, true));
    
    //temp code for debugging rules
    _log->debug("All FC Rules:");
    for (Rule* rule : _fcmem.get_rules()) {
        _log->debug(rule->get_name().c_str());
        //_log->debug(getFormulaName(rule));
        //_log->debug(NodeCast((LinkCast(rule->get_implicand()))->getOutgoingSet()[0])->getName());     //toShortString().c_str()); //toString().c_str());
    }

    cerr << "logger stuff to fc.log should have happened above" << endl;
}


void ForwardChainer::setLogger(Logger* log)
{
    if (_log)
        delete _log;
    _log = log;
}

Logger* ForwardChainer::getLogger()
{
    return _log;
}

///**
// * Choose a rule to apply to a forward chaining step based on a previously
// * selected source. Return the rule partially grounded by the source. If no
// * rules can be unified with the current source, NULL is returned.
// *
// * 1. Stochastically choose a non-previously-chosen rule from the rule base
// * 2. Attempt to ground rule with source (if fails, choose new rule)
// * 3. Return rule grounded by source
// */
//Rule * ForwardChainer::get_grounded_rule(Handle source,
//                                        ForwardChainerCallBack& fcb) {
//    vector<Rule*> candidate_rules = _fcmem.get_rules();
//
//    map<Rule*, float> rule_weight_map;
//    for (Rule* r : candidate_rules) {
//        rule_weight_map[r] = r->get_weight();
//    }
//
//    _log->info("[ForwardChainer] Selecting a rule from the set of "
//                       "candidate rules.");
//
//    while (!rule_weight_map.empty()) {
//        Rule *r = _rec.tournament_select(rule_weight_map);
//
//        HandleSeq hs = r->get_implicand_seq();
//        for(Handle target: hs)
//        {
//            HandleSeq hs = fcb.unify(source,target,r);
//            derived_rules.insert(derived_rules.end(),hs.begin(),hs.end());
//        }
//        //Chosen rule.
//        if (not derived_rules.empty())
//        {
//            chosen_rules.push_back(rule);
//            rule_derivations[rule->get_handle()] = derived_rules;
//        }
//
//    }
//
//}



/**
 * Does a single step of forward chaining based on a previously chosen source.
 *
 * @param fcb a concrete implementation of of ForwardChainerCallBack class
 */
void ForwardChainer::do_step_new(ForwardChainerCallBack& fcb) {

    Handle cur_source = _fcmem.get_cur_source();
    if (cur_source == Handle::UNDEFINED) {
        _log->debug("do_step(): No current source set in _fcmem ");
        return;
    }

    _log->info("[ForwardChainer] Current source %s",
               cur_source->toShortString().c_str());

    pair<Handle,string> grounded_rule = fcb.get_grounded_rule(cur_source,_fcmem);
    Handle grounded_bl = grounded_rule.first;
    string rule_name = grounded_rule.second;
    if (grounded_bl == Handle::UNDEFINED) {
        _log->debug("No rule unifies with current source.");
        return;
    }

    _log->debug("Applying rule: %s",rule_name.c_str());
    _log->fine("grounded rule: %s",grounded_bl->toShortString().c_str());
grounded_bl = _as.add_atom(grounded_bl);
    HandleSeq product = fcb.evaluate_bindlink(grounded_bl, _fcmem);

    _log->info("Conclusions: %i", product.size());
    for(const auto& p:product)
    {
        _log->info("%s ", p->toShortString().c_str() );
    }

    //_log->info("[ForwardChainer] updating premise list with the "
    //                   "inference made");
    _fcmem.update_potential_sources(product);

    //_log->info("[ForwardChainer] adding inference to history");
    _fcmem.add_rules_product(_iteration, product);



}


/**
 * Does one step forward chaining
 *
 * @return An unordered sets of result of applying a particular selected rule.
 */
UnorderedHandleSet ForwardChainer::do_step(bool search_focus_set/* = false*/)
{

    Handle hsource = choose_next_source(_fcmem);

    _log->debug("[ForwardChainer] Next source %s", hsource->toString().c_str());

    _fcmem.set_source(hsource);

    HandleSeq derived_rhandles;

    //choose a rule that source unifies with one of its premises.
    Rule *rule = choose_rule(hsource, false);
    if (rule) {
        _fcmem.set_cur_rule(rule);
        derived_rhandles = derive_rules(hsource, rule);

/* my stuff i tried
    _log->debug("matched_rules:");
    for (auto rule : matched_rules) {
        _log->debug("     %s", rule->get_name().c_str());
    }

    // Select a rule amongst the matching rules by tournament selection
    map<Rule*, float> rule_weight;
    for (Rule* r : matched_rules) {
        rule_weight[r] = r->get_weight();
    }

    _log->info("[ForwardChainer] Selecting a rule from the set of "
               "candidate rules.");
    Rule* r = _rec.tournament_select(rule_weight);
    _fcmem.set_cur_rule(r);
    //_log->info("[ForwardChainer] Selected rule is %s", (r->get_handle())->toShortString().c_str());
    _log->debug("Current rule: %s", r->get_name().c_str());
    _log->debug("[ForwardChainer] Selected rule formula is %s", getFormulaName(r));
*/

    } else {
        //choose rule that unifies that source unifies with sub-atoms of its premises.
        rule = choose_rule(hsource, true);

        if (rule) {
            _fcmem.set_cur_rule(rule);
            derived_rhandles = derive_rules(hsource, rule,
            true);
        }
    }

    _log->debug( "Derived rule size = %d", derived_rhandles.size());

    UnorderedHandleSet products;

/* more my stuff i tried

    //! Apply rule.
    _log->debug("[ForwardChainer] Applying chosen rule %s",
               //(r->get_handle())->toShortString().c_str());
                getFormulaName(r));


    _log->debug("Applying rule, _fcmem current rule: %s",
                _fcmem.get_cur_rule()->get_handle()->toShortString().c_str());


    HandleSeq product = fcb.apply_rule(_fcmem);

    _log->info("NEW PRODUCTS: %i", product.size());
    for(const auto& p:product)
    {
        _log->info("%s ", p->toShortString().c_str() );
*/

    for (Handle rhandle : derived_rhandles) {
        HandleSeq temp_result = apply_rule(rhandle,search_focus_set);

        std::copy(temp_result.begin(), temp_result.end(),
                  std::inserter(products, products.end()));
    }

    return products;
}

/**
 * Specialized stepper for bio project
 * Does one step forward chaining applying all rules
 *
 * @param fcb a concrete implementation of of ForwardChainerCallBack class
 */
void ForwardChainer::do_step_bio(ForwardChainerCallBack& fcb)
{
    _log->fine("FowardChainer::do_step_bio()");

    if (_fcmem.get_cur_source() == Handle::UNDEFINED) {
        _log->info("[ForwardChainer] No current source, step "
                           "forward chaining aborted.");
        return;
    }

    _log->info("[ForwardChainer] Next source %s",
               _fcmem.get_cur_source()->toString().c_str());

    // Choose matching rules whose input matches with the source.
    //vector<Rule*> matched_rules = fcb.choose_rules(_fcmem);

    //! If no rules matches the pattern of the source,
    //! set all rules for candidacy to be selected by the proceeding step.
    //! xxx this decision is maded based on recent discussion.I
    //! it might still face some changes.
//    if (matched_rules.empty()) {
//        _log->info("[ForwardChainer] No matching rule found. "
//                           "Setting all rules as candidates.");
//        matched_rules = _fcmem.get_rules();
//    }

//    // Select a rule amongst the matching rules by tournament selection
//    map<Rule*, float> rule_weight;
//    for (Rule* r : matched_rules) {
//        rule_weight[r] = r->get_cost();
//    }
//
//    _log->info("[ForwardChainer] Selecting a rule from the set of "
//                       "candidate rules.");
//    auto r = _rec.tournament_select(rule_weight);
//    _fcmem.set_cur_rule(r);
//    _log->info("[ForwardChainer] Selected rule is %s", r->get_name().c_str());



//    if (_fcmem.get_cur_source() == Handle::UNDEFINED) {
//        _log->info(
//                "[ForwardChainer] No current source, forward chaining aborted");
//        return false;
//    }

    /*
     * choose rule(s)
     * match premises
     */

    //crashing
    //current_source = choose_source();

    Handle current_source, original_source;
    current_source = original_source = _fcmem.get_cur_source();

    _log->fine("[ForwardChainer] Current source:\n %s",
                current_source->toShortString().c_str());

    //let's start with just iterating through all the rules
    //vector<Rule*> chosen_rules = fccb->choose_rules(current_source);
    vector<Rule*> chosen_rules = _fcmem.get_rules();
    _log->fine("%i rules chosen for source", chosen_rules.size());

    //for now we are just handling case of source being a single node
    if (!NodeCast(current_source)) {
        _log->info("Link found for source, not implemented yet.");
        return; // return false;
    }

    // apply each rule at each step
    for (Rule* rule : chosen_rules) {

        // I think below will go into EMForwardChainerCB::apply_rule()
        _log->debug("Current rule: %s", rule->get_name().c_str());
        _log->debug(
                "============================================================================");
        _log->fine(rule->get_handle()->toShortString().c_str());

        Handle var_listlink = rule->get_vardecl();
        HandleSeq var_seq = LinkCast(var_listlink)->getOutgoingSet();

        //BindLink no longer contains an implication link
        // Handle implication_link = LinkCast(rule->get_handle())->getOutgoingSet()[1];
        // Handle implicator_link = LinkCase(rule->get_handle())->getOutgoingSet()[1]
        Handle implicant = rule->get_implicant();
        Handle implicand = rule->get_implicand();

        //for now we are just handling case of source being a single node
        //for each variablenode, substitute the source, and then do unification
        // TODO: need to handle typed variables
        for (Handle current_varnode :  LinkCast(
                var_listlink)->getOutgoingSet()) {
            //for abstraction var $C is hanging out due to combo explosion of conclusions
            //so leaving it out for now pending a better solution
            //        printf("rule=abduction: %s %i",rule->get_name().c_str(),strcmp(rule->get_name(),"pln-rule-abduction");
            //        printf("   varnode = $C: %s %i",NodeCast(current_varnode)->getName(),
            //               strncmp(NodeCast(current_varnode)->getName(),"$C"));
            //        if (rule->get_name().compare("pln-rule-abduction")==0
            //            && NodeCast(current_varnode)->getName().compare("$C")==0) {
            //            continue;
            //        }

            _log->debug("grounding rule variable: %s",
                        current_varnode->toShortString().c_str());

            // Could handle special case here for abduction (and probably others) to only substitute
            // for one of Variable A or Variable B, since they are equivalent. Oh wait, that's
            // probably not true because the resulting Inheritance could go both ways.
            // Though are AndLinks ordered? I noticed in the order for the abduction implicant
            // AndLink was reversed from the rule in scheme file definition in debug output.

            map<Handle, Handle> replacement_map = map<Handle, Handle>();
            replacement_map[current_varnode] = current_source;
            //this is actually the grounded implication part of the rule (w/out the variables)
            //had problems gou
            // Handle grounded_rule = ure_commons.change_node_types(implication_link,replacement_map);
            Handle grounded_implicant = _rec.change_node_types(implicant,
                                                               replacement_map);
            Handle grounded_implicand = _rec.change_node_types(implicand,
                                                               replacement_map);
            //            Handle rule_handle = rule->get_handle();
            //            Handle grounded_rule = ure_commons.change_node_types(rule_handle,replacement_map);

            //need to remove the source from the variable list
            HandleSeq vars;
            for (auto varnode : var_seq) {
                if (varnode != current_varnode) {
                    vars.push_back(varnode);
                }
            }
            Handle varlist = Handle(createVariableList(vars));

            HandleSeq bl_contents = {varlist, grounded_implicant,
                                     grounded_implicand};

            //            Handle hbindlink = ure_commons.create_bindlink_from_implicationlink(grounded_rule);
            //BindLink bl(bl_contents);
            BindLinkPtr bl_p = createBindLink(bl_contents);
            //Handle bl_h = Handle(*LinkCast(bl));
            Handle bl_h = Handle(bl_p);

            _log->fine("The grounded bindlink:\n%s",
                       bl_h->toShortString().c_str());


            //BindLinkPtr pln_bindlink(BindLinkCast(hbindlink));


            DefaultImplicator impl(&_as);
            impl.max_results = MAX_PM_RESULTS;
            //TODO: should use PLN_Implicator or pln_bindlink, but it's not working (returns empty set)
            //HandleSeq rule_conclusions = LinkCast(bindlink(as,bl_h))->getOutgoingSet();
//            HandleSeq rule_conclusions = LinkCast(
//                    do_imply(&_as, bl_h, impl))->getOutgoingSet();
            HandleSeq rule_conclusions;

            BindLinkPtr bl(BindLinkCast(bl_h));
            if (NULL == bl) {
                bl = createBindLink(*LinkCast(bl_h));
            }
            impl.implicand = bl->get_implicand();
            bl->imply(impl,false);
            //rule_conclusions = _as.add_link(SET_LINK, impl.result_list);
            rule_conclusions = impl.get_result_list();


            //            BindLinkPtr bindlink(BindLinkCast(grounded_rule));
            //            logger.fine("The grounded bindlink:\n%s",pln_bindlink->toShortString().c_str());
            //            DefaultImplicator implicator(as);
            //            implicator.implicand = pln_bindlink->get_implicand();
            //            pln_bindlink->imply(implicator);
            //
            //            vector<Handle> concl = implicator.result_list;
            //            set<Handle> conclutions(concl.begin(), concl.end());
            int concl_size = rule_conclusions.size();


            //add the particular rule's conclusions to the full list of conclusions
            std::copy(rule_conclusions.begin(), rule_conclusions.end(),
                      std::inserter(conclusions, conclusions.end()));

            _log->debug("----------------------------------------------");
            _log->debug("Generated %i conclusions\n", concl_size);
            string var_name = NodeCast(current_varnode)->getName().c_str();

            if (_log->getLevel() == Logger::getLevelFromString("FINE")) {
                if (concl_size > 0 and concl_size <= 100) {
                    string conclStr = var_name + " Conclusions:\n";
                    //logger.debug("Conclusions:");
                    for (auto conclusion : rule_conclusions) {
                        //logger.debug(conclusion->toShortString().c_str());
                        conclStr = conclStr + "\n" +
                                   conclusion->toShortString().c_str();
                    }
                    _log->fine(conclStr);
                }
                else if (concl_size > 100) {
                    _log->debug("%s - too many conclusions to list out\n",
                                NodeCast(current_varnode)->getName().c_str());
                    //logger.debug("plus %i more conclusions", concl_size - 100);
                }
                else { // (concl_size == 0) {
                    _log->debug("%s Conclusions: None\n",
                                NodeCast(current_varnode)->getName().c_str());
                }
            }
        }
    }


    return;  //return true;

//
//    //!TODO Find/add premises?
//
//    //! Apply rule.
//    _log->info("[ForwardChainer] Applying chosen rule %s",
//               r->get_name().c_str());
//    HandleSeq product = fcb.apply_rule(_fcmem);
//
//    _log->info("PRODUCTS...");
//    for(const auto& p:product)
//    {
//        _log->info("%s ", p->toShortString().c_str() );
//    }
//
//    _log->info("[ForwardChainer] updating premise list with the "
//                       "inference made");
//    _fcmem.update_potential_sources(product);

//    _log->info("[ForwardChainer] adding inference to history");
//    _fcmem.add_rules_product(_iteration, product);
}


void ForwardChainer::do_chain(Handle hsource, HandleSeq focus_set)
{

    validate(hsource,focus_set);

    _fcmem.set_focus_set(focus_set);

    HandleSeq init_sources;
    //Accept set of initial sources wrapped in a SET_LINK
    if(LinkCast(hsource) and hsource->getType() == SET_LINK)
    {
     init_sources = _as.get_outgoing(hsource);

     //Relex2Logic uses this.TODO make a separate class
     //to handle this robustly.
     if(init_sources.empty())
     {
         bool search_in_af = not focus_set.empty();
         apply_all_rules(search_in_af);
         return;
     }

    }
    else
    {
        init_sources.push_back(hsource);
    }

    // Variable fulfillment query.
    UnorderedHandleSet var_nodes = get_outgoing_nodes(hsource,
                                                      { VARIABLE_NODE });
    if (not var_nodes.empty())
        return do_pm(hsource, var_nodes);

    // Default forward chaining
    _fcmem.update_potential_sources(init_sources);

    auto max_iter = _configReader.get_maximum_iterations();

    while (_iteration < max_iter /*OR other termination criteria*/) {

        //do_step_new(fcb);
        
	_log->debug("Iteration %d", _iteration);

        UnorderedHandleSet products;

        if (focus_set.empty())
            products = do_step(false);
        else
            products = do_step(true);

        _fcmem.add_rules_product(_iteration,
                                 HandleSeq(products.begin(), products.end()));
        _fcmem.update_potential_sources(
                HandleSeq(products.begin(), products.end()));

        _iteration++;
    }

    _log->debug("[ForwardChainer] finished forwarch chaining.");
}


void ForwardChainer::do_chain_bio(ForwardChainerCallBack& fcb,
                              Handle hsource/*=Handle::UNDEFINED*/)
{
    if (hsource == Handle::UNDEFINED) {
        do_pm();
        return;
    }
    // Variable fulfillment query.
    UnorderedHandleSet var_nodes = get_outgoing_nodes(hsource,
                                                      { VARIABLE_NODE });
    if (not var_nodes.empty())
        return do_pm(hsource, var_nodes, fcb);

    auto max_iter = _configReader.get_maximum_iterations();
    _fcmem.set_source(hsource); //set initial source
    _fcmem.update_potential_sources(HandleSeq{hsource});

    while (_iteration < max_iter /*OR other termination criteria*/) {
        _log->info("Iteration %d", _iteration);

        do_step_bio(fcb);

        //! Choose next source.
        _log->info("[ForwardChainer] setting next source");
        _fcmem.set_source(fcb.choose_next_source(_fcmem));

        _iteration++;
    }

    _log->info("[ForwardChainer] finished do_chain.");
}



/**
 * Does pattern matching for a variable containing query.
 *
 * @param source    A variable containing handle passed as an input to the pattern matcher
 * @param var_nodes The VariableNodes in @param hsource
 *
 */
void ForwardChainer::do_pm(const Handle& hsource,
                           const UnorderedHandleSet& var_nodes)
{
    DefaultImplicator impl(&_as);
    impl.implicand = hsource;
    HandleSeq vars;
    for (auto h : var_nodes)
        vars.push_back(h);
    _fcmem.set_source(hsource);
    Handle hvar_list = _as.add_link(VARIABLE_LIST, vars);
    Handle hclause = _as.add_link(AND_LINK, hsource);

    // Run the pattern matcher, find all patterns that satisfy the
    // the clause, with the given variables in it.
    PatternLinkPtr sl(createPatternLink(hvar_list, hclause));
    sl->satisfy(impl);

    // Update result
    _fcmem.add_rules_product(0, impl.get_result_list());

    // Delete the AND_LINK and LIST_LINK
    _as.remove_atom(hvar_list);
    _as.remove_atom(hclause);

    //!Additionally, find applicable rules and apply.
    vector<Rule*> rules = {choose_rule(hsource,true)};
    for (Rule* rule : rules) {
        BindLinkPtr bl(BindLinkCast(rule->get_handle()));
        DefaultImplicator impl(&_as);
        impl.implicand = bl->get_implicand();
        bl->imply(impl);
        _fcmem.set_cur_rule(rule);
        _fcmem.add_rules_product(0, impl.get_result_list());
    }
}

/**
 * Applies all rules in the rule base.
 *
 * @param search_focus_set flag for searching focus set.
 */
void ForwardChainer::apply_all_rules(bool search_focus_set /*= false*/)
{
    vector<Rule*> rules = _fcmem.get_rules();

    for (Rule* rule : rules) {
        _fcmem.set_cur_rule(rule);
        HandleSeq hs = apply_rule(rule->get_handle(), search_focus_set);

        //Update
        _fcmem.add_rules_product(0, hs);
        _fcmem.update_potential_sources(hs);
    }

}

HandleSeq ForwardChainer::get_chaining_result()
{
    return _fcmem.get_result();
}

HandleSeq ForwardChainer::get_conclusions()
{
    HandleSeq result;

    for (auto h : conclusions) {
        result.push_back(h);
    }

    return result;
}

Rule* ForwardChainer::choose_rule(Handle hsource, bool subatom_match)
{
    //TODO move this somewhere else
    std::map<Rule*, float> rule_weight;
    for (Rule* r : _fcmem.get_rules())
        rule_weight[r] = r->get_weight();

    _log->debug("[ForwardChainer] %d rules to be searched",rule_weight.size());

    //Select a rule among the admissible rules in the rule-base via stochastic
    //selection,based on the weights of the rules in the current context.
    Rule* rule = nullptr;
    bool unifiable = false;

    if (subatom_match) {
        _log->debug("[ForwardChainer] Subatom-unifying. %s",(hsource->toShortString()).c_str());

        while (!unifiable and !rule_weight.empty()) {
            Rule* temp = _rec.tournament_select(rule_weight);

            if (subatom_unify(hsource, temp)) {
                unifiable = true;
                rule = temp;
                break;
            }
            rule_weight.erase(temp);
        }

    } else {
        _log->debug("[ForwardChainer] Unifying. %s",(hsource->toShortString()).c_str());

        while (!unifiable and !rule_weight.empty()) {
            Rule *temp = _rec.tournament_select(rule_weight);
            HandleSeq hs = temp->get_implicant_seq();

            for (Handle target : hs) {
                if (unify(hsource, target, temp)) {
                    unifiable = true;
                    rule = temp;
                    break;
                }
            }
            rule_weight.erase(temp);
        }
    }

    if(nullptr != rule)
        _log->debug("[ForwardChainer] Selected rule is %s",
                            (rule->get_handle())->toShortString().c_str());
    else
       _log->debug("[ForwardChainer] No match found.");

    return rule;
};

HandleSeq ForwardChainer::choose_premises(FCMemory& fcmem)
{
    HandleSeq inputs;
    URECommons urec(_as);
    Handle hsource = fcmem.get_cur_source();

    // Get everything associated with the source handle.
    UnorderedHandleSet neighbors = get_distant_neighbors(hsource, 2);

    // Add all root links of atoms in @param neighbors.
    for (auto hn : neighbors) {
        if (hn->getType() != VARIABLE_NODE) {
            HandleSeq roots;
            urec.get_root_links(hn, roots);
            for (auto r : roots) {
                if (find(inputs.begin(), inputs.end(), r) == inputs.end() and r->getType()
                        != BIND_LINK)
                    inputs.push_back(r);
            }
        }
    }

    return inputs;
}

Handle ForwardChainer::choose_next_source(FCMemory& fcmem)
{

    URECommons urec(_as);
    HandleSeq tlist = fcmem.get_potential_sources();
    map<Handle, float> tournament_elem;

    switch (_ts_mode) {
    case TV_FITNESS_BASED:
        for (Handle t : tlist)
            tournament_elem[t] = urec.tv_fitness(t);
        break;

    case STI_BASED:
        for (Handle t : tlist)
            tournament_elem[t] = t->getSTI();
        break;

    default:
        throw RuntimeException(TRACE_INFO, "Unknown source selection mode.");
        break;
    }

    Handle hchosen = Handle::UNDEFINED;

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

HandleSeq ForwardChainer::apply_rule(Handle rhandle,bool search_in_focus_set /*=false*/)
{
    HandleSeq result;

    if (search_in_focus_set) {
        //This restricts PM to look only in the focus set
        AtomSpace focus_set_as;

        //Add focus set atoms to focus_set atomspace
        HandleSeq focus_set_atoms = _fcmem.get_focus_set();
        for (Handle h : focus_set_atoms)
            focus_set_as.add_atom(h);

        //Add source atoms to focus_set atomspace
        HandleSeq sources = _fcmem.get_potential_sources();
        for (Handle h : sources)
            focus_set_as.add_atom(h);

        //rhandle may introduce a new atoms that satisfies condition for the output
        //In order to prevent this undesirable effect, lets store rhandle in a child
        //atomspace of parent focus_set_as so that PM will never be able to find this
        //new undesired atom created from partial grounding.
        AtomSpace derived_rule_as(&focus_set_as);
        Handle rhcpy = derived_rule_as.add_atom(rhandle);

        BindLinkPtr bl = BindLinkCast(rhcpy);

        FocusSetPMCB fs_pmcb(&derived_rule_as, &_as);
        fs_pmcb.implicand = bl->get_implicand();

        _log->debug("Applying rule in focus set %s ",(rhcpy->toShortString()).c_str());

        bl->imply(fs_pmcb);

        result = fs_pmcb.get_result_list();

        _log->debug(
                "Result is %s ",
                ((_as.add_link(SET_LINK, result))->toShortString()).c_str());

    }
    //Search the whole atomspace
    else {
        AtomSpace derived_rule_as(&_as);

        Handle rhcpy = derived_rule_as.add_atom(rhandle);

        _log->debug("Applying rule on atomspace %s ",(rhcpy->toShortString()).c_str());

        Handle h = bindlink(&derived_rule_as,rhcpy);

        _log->debug("Result is %s ",(h->toShortString()).c_str());

        result = derived_rule_as.get_outgoing(h);
    }

    //add the results back to main atomspace
    for(Handle h:result) _as.add_atom(h);

    return result;
}

/**
 * Derives new rules by replacing variables that are unfiable in @param target
 * with source.The rule handles are not added to any atomspace.
 *
 * @param  source    A source atom that will be matched with the target.
 * @param  target    An implicant containing variable to be grounded form source.
 * @param  rule      A rule object that contains @param target in its implicant. *
 *
 * @return  A HandleSeq of derived rule handles.
 */
HandleSeq ForwardChainer::derive_rules(Handle source, Handle target,
                                               Rule* rule)
{
    //exceptions
    if (not is_valid_implicant(target))
        return {};

    HandleSeq derived_rules = { };

    AtomSpace temp_pm_as;
    Handle hcpy = temp_pm_as.add_atom(target);
    Handle implicant_vardecl = temp_pm_as.add_atom(
            gen_sub_varlist(target, rule->get_vardecl()));
    Handle sourcecpy = temp_pm_as.add_atom(source);

    Handle h = temp_pm_as.add_link(BIND_LINK,HandleSeq { implicant_vardecl, hcpy, hcpy });
    BindLinkPtr bl = BindLinkCast(h);

    VarGroundingPMCB gcb(&temp_pm_as);
    gcb.implicand = bl->get_implicand();

    bl->imply(gcb);

    auto del_by_value =
            [] (std::vector<std::map<Handle,Handle>>& vec_map,const Handle& h) {
                for (auto& map: vec_map)
                for(auto& it:map) {if (it.second == h) map.erase(it.first);}
            };

    //We don't want VariableList atoms to ground free-vars.
    del_by_value(gcb.term_groundings, implicant_vardecl);
    del_by_value(gcb.var_groundings, implicant_vardecl);

    FindAtoms fv(VARIABLE_NODE);
    for (const auto& termg_map : gcb.term_groundings) {
        for (const auto& it : termg_map) {
            if (it.second == sourcecpy) {

                fv.search_set(it.first);

                Handle rhandle = rule->get_handle();
                HandleSeq new_candidate_rules = substitute_rule_part(
                        temp_pm_as, temp_pm_as.add_atom(rhandle), fv.varset,
                        gcb.var_groundings);

                for (Handle nr : new_candidate_rules) {
                    if (find(derived_rules.begin(), derived_rules.end(), nr) == derived_rules.end()
                            and nr != rhandle) {
                        derived_rules.push_back(nr);
                    }
                }
            }
        }
    }

    return derived_rules;
}

/**
 * Derives new rules by replacing variables in @param rule that are
 * unifiable with source.
 *
 * @param  source    A source atom that will be matched with the rule.
 * @param  rule      A rule object
 * @param  subatomic A flag that sets subatom unification.
 *
 * @return  A HandleSeq of derived rule handles.
 */
HandleSeq ForwardChainer::derive_rules(Handle source, Rule* rule,
                                               bool subatomic/*=false*/)
{
    HandleSeq derived_rules = { };

    auto add_result = [&derived_rules] (HandleSeq result) {
        derived_rules.insert(derived_rules.end(), result.begin(),
                result.end());
    };

    if (subatomic) {
        for (Handle target : get_subatoms(rule))
            add_result(derive_rules(source, target, rule));

    } else {
        for (Handle target : rule->get_implicant_seq())
            add_result(derive_rules(source, target, rule));

    }

    return derived_rules;
}

/**
 * Checks whether an atom can be used to generate a bindLink or not.
 *
 * @param h  The atom handle to be validated.
 *
 * @return   A boolean result of the check.
 */
bool ForwardChainer::is_valid_implicant(const Handle& h)
{
    FindAtoms fv(VARIABLE_NODE);
            fv.search_set(h);

    bool is_valid = (h->getType() != NOT_LINK)   and
                    (not classserver().isA(h->getType(),
                                          VIRTUAL_LINK))
                    and
                    (not fv.varset.empty());

    return is_valid;
}

void ForwardChainer::validate(Handle hsource, HandleSeq hfocus_set)
{
    if (hsource == Handle::UNDEFINED)
        throw RuntimeException(TRACE_INFO, "ForwardChainer - Invalid source.");
   //Any other validation here
}

/**
 * Gets all unique atoms of in the implicant list of @param r.
 *
 * @param r  A rule object
 *
 * @return   An unoderedHandleSet of of all unique atoms in the implicant.
 */
UnorderedHandleSet ForwardChainer::get_subatoms(Rule *rule)
{
    UnorderedHandleSet output_expanded;

    HandleSeq impl_members = rule->get_implicant_seq();
    for (Handle h : impl_members) {
        UnorderedHandleSet hs = get_all_unique_atoms(h);
        hs.erase(h); //Already tried to unify this.
        output_expanded.insert(hs.begin(), hs.end());
    }

    return output_expanded;
}

/**
 * Derives new rules from @param hrule by replacing variables
 * with their groundings.
 *
 * @param as             An atomspace where the handles dwell.
 * @param hrule          A handle to BindLink instance
 * @param vars           The grounded var list in @param hrule
 * @param var_groundings The set of groundings to each var in @param vars
 *
 * @return A HandleSeq of all possible derived rules
 */
HandleSeq ForwardChainer::substitute_rule_part(
        AtomSpace& as, Handle hrule, const std::set<Handle>& vars,
        const std::vector<std::map<Handle, Handle>>& var_groundings)
{
    std::vector<std::map<Handle, Handle>> filtered_vgmap_list;

    //Filter out variables not listed in vars from var-groundings
    for (const auto& varg_map : var_groundings) {
        std::map<Handle, Handle> filtered;

        for (const auto& iv : varg_map) {
            if (find(vars.begin(), vars.end(), iv.first) != vars.end()) {
                filtered[iv.first] = iv.second;
            }
        }

        filtered_vgmap_list.push_back(filtered);
    }

    HandleSeq derived_rules;
    BindLinkPtr blptr = BindLinkCast(hrule);
    //Substitutor st(&as);

    for (auto& vgmap : filtered_vgmap_list) {
        Handle himplicand = Substitutor::substitute(blptr->get_implicand(), vgmap);
        //Create the BindLink/Rule by substituting vars with groundings
        if (contains_atomtype(himplicand, VARIABLE_NODE)) {
            Handle himplicant = Substitutor::substitute(blptr->get_body(), vgmap);

            //Assuming himplicant's set of variables are superset for himplicand's,
            //generate varlist from himplicant.
            Handle hvarlist = as.add_atom(gen_sub_varlist(
                    himplicant, LinkCast(hrule)->getOutgoingSet()[0]));
            Handle hderived_rule = as.add_atom(Handle(createBindLink(HandleSeq {
                    hvarlist, himplicant, himplicand })));
            derived_rules.push_back(hderived_rule);
        } else {
            //TODO Execute if executable and push to FC results
        }
    }

    return derived_rules;
}

/**
 * Tries to unify the @param source with @parama target and derives
 * new rules using @param rule as a template.
 *
 * @param source  An atom that might bind to variables in @param rule.
 * @param target  An atom to be unified with @param source
 * @rule  rule    The rule object whose implicants are to be unified.
 *
 * @return        true on successful unification and false otherwise.
 */
bool ForwardChainer::unify(Handle source, Handle target, Rule* rule)
{
    //exceptions
    if (not is_valid_implicant(target))
        return false;

    AtomSpace temp_pm_as;
    Handle hcpy = temp_pm_as.add_atom(target);
    Handle implicant_vardecl = temp_pm_as.add_atom(
            gen_sub_varlist(target, rule->get_vardecl()));
    Handle sourcecpy = temp_pm_as.add_atom(source);

    BindLinkPtr bl =
    createBindLink(HandleSeq { implicant_vardecl, hcpy, hcpy });
    Handle blhandle = temp_pm_as.add_atom(bl);
    Handle  result = bindlink(&temp_pm_as, blhandle);
    HandleSeq results = temp_pm_as.get_outgoing(result);

    if (std::find(results.begin(), results.end(), sourcecpy) != results.end())
        return true;
    else
        return false;
}

/**
 *  Checks if sub atoms of implicant lists in @param rule are unifiable with
 *  @param source.
 *
 *  @param source  An atom that might bind to variables in @param rule.
 *  @param rule    The rule object whose implicants are to be sub atom unified.
 *
 *  @return        true if source is subatom unifiable and false otherwise.
 */
bool ForwardChainer::subatom_unify(Handle source, Rule* rule)
{
    UnorderedHandleSet output_expanded = get_subatoms(rule);

    for (Handle h : output_expanded) {
        if (unify(source, h, rule))
            return true;
    }

    return false;
}

Handle ForwardChainer::gen_sub_varlist(const Handle& parent,
                                       const Handle& parent_varlist)
{
    FindAtoms fv(VARIABLE_NODE);
    fv.search_set(parent);

    HandleSeq oset;
    if (LinkCast(parent_varlist))
        oset = LinkCast(parent_varlist)->getOutgoingSet();
    else
        oset.push_back(parent_varlist);

    HandleSeq final_oset;

    // for each var in varlist, check if it is used in parent
    for (const Handle& h : oset) {
        Type t = h->getType();

        if (VARIABLE_NODE == t && fv.varset.count(h) == 1)
            final_oset.push_back(h);
        else if (TYPED_VARIABLE_LINK == t
                and fv.varset.count(LinkCast(h)->getOutgoingSet()[0]) == 1)
            final_oset.push_back(h);
    }

    return Handle(createVariableList(final_oset));
}
>>>>>>> d8f214d24ce588c84eaf86d555dba1dc5c8e6e22
