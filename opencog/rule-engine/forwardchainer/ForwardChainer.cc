/*
 * ForwardChainer.cc
 *
 * Copyright (C) 2014,2015 OpenCog Foundation
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

#include <boost/range/algorithm/find.hpp>

#include <opencog/atoms/execution/Instantiator.h>
#include <opencog/atoms/pattern/BindLink.h>
#include <opencog/atoms/pattern/PatternLink.h>
#include <opencog/atomutils/AtomUtils.h>
#include <opencog/atomutils/FindUtils.h>
#include <opencog/atomutils/Substitutor.h>
#include <opencog/query/BindLinkAPI.h>
#include <opencog/query/DefaultImplicator.h>
#include <opencog/rule-engine/Rule.h>
#include <opencog/util/Logger.h>

#include "ForwardChainer.h"
#include "FocusSetPMCB.h"
#include "VarGroundingPMCB.h"

using namespace opencog;

/*  bio old
int MAX_PM_RESULTS = 10000;

ForwardChainer::ForwardChainer(AtomSpace& as, Handle rbs) :
        _as(as), _rec(as), _rbs(rbs), _configReader(as, rbs), _fcmem(&as)
{
    //cout << "ForwardChainer::ForwardChainer()" << endl;
    init();
}

// temporary util until rule names are fixed
const char * getFormulaName(Rule *rule) {
    return NodeCast((LinkCast(rule->get_implicand()))->getOutgoingSet()[0])->getName().c_str();
}

void ForwardChainer::init()
{

    //cout << "ForwardChainer::init()" << endl;

    _fcmem.set_search_in_af(_configReader.get_attention_allocation());
    _fcmem.set_rules(_configReader.get_rules());
    _fcmem.set_cur_rule(nullptr);

*/

ForwardChainer::ForwardChainer(AtomSpace& as, Handle rbs, Handle hsource,
                               HandleSeq focus_set) :
        _as(as), _rec(as), _rbs(rbs), _configReader(as, rbs)
{
    init(hsource,focus_set);
}

ForwardChainer::~ForwardChainer()
{

}

void ForwardChainer::init(Handle hsource, HandleSeq focus_set)
{
     validate(hsource, focus_set);

    _search_in_af = _configReader.get_attention_allocation();
    _search_focus_Set = not focus_set.empty();
    _ts_mode = TV_FITNESS_BASED;

    _focus_set = focus_set;

    //Set potential source.
    HandleSeq init_sources = { };
    //Accept set of initial sources wrapped in a SET_LINK
    if (LinkCast(hsource) and hsource->getType() == SET_LINK) {
        init_sources = _as.get_outgoing(hsource);
    } else {
        init_sources.push_back(hsource);
    }
    update_potential_sources(init_sources);

     //Set rules.
     for(Rule& r :_configReader.get_rules())
     {
         _rules.push_back(&r);
     }
    _cur_rule = nullptr;
    
    // Provide a logger
    _log = NULL;
    setLogger(new opencog::Logger("forward_chainer.log", Logger::FINE, true));
    
/*    //temp code for debugging rules
    _log->debug("All FC Rules:");
    for (Rule* rule : _fcmem.get_rules()) {
        _log->debug(rule->get_name().c_str());
        //_log->debug(getFormulaName(rule));
        //_log->debug(NodeCast((LinkCast(rule->get_implicand()))->getOutgoingSet()[0])->getName());     //toShortString().c_str()); //toString().c_str());
    }
*/
    //cerr << "logger stuff to fc.log should have happened above" << endl;
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
/*  needs to be refactored without ForwardChainerCallBack class which has benn removed
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
*/

/**
 * Does one step forward chaining and stores result.
 *
 */
void ForwardChainer::do_step(void)
{
    _cur_source = choose_next_source();
    _log->debug("[ForwardChainer] Next source %s",
                _cur_source->toString().c_str());

    const Rule* rule;
    HandleSeq derived_rhandles = { };

    auto get_derived = [&](Handle hsource) {
        auto pgmap = _fcstat.get_rule_pg_map(hsource);
        auto it = pgmap.find(rule->get_handle());
        if (it != pgmap.end()) {
            for (auto hwm : it->second) {
                derived_rhandles.push_back(hwm.first);
            }
        }
    };

    //Look in the cache first
    map<Rule, float> rule_weight_map;
    rule = choose_rule(_cur_source, false);

    if (rule) {
        _cur_rule = rule;
        if (_fcstat.has_partial_grounding(_cur_source))
            get_derived(_cur_source);
        else
            derived_rhandles = derive_rules(_cur_source, rule);

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
        rule = choose_rule(_cur_source, true);
        if (rule) {
            _cur_rule = rule;
            if (_fcstat.has_partial_grounding(_cur_source))
                get_derived(_cur_source);
            else
                derived_rhandles = derive_rules(_cur_source, rule, true);
        }
    }

    _log->debug("Derived rule size = %d", derived_rhandles.size());

/* more my stuff i tried

    UnorderedHandleSet products;

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

    UnorderedHandleSet products;
    //Applying all partial/full groundings.
    for (Handle rhandle : derived_rhandles) {
        HandleSeq hs = apply_rule(rhandle, _search_focus_Set);
        products.insert(hs.begin(), hs.end());
    }

    //Finally store source partial groundings and inference results.
    if (not derived_rhandles.empty()) {
        _potential_sources.insert(_potential_sources.end(), products.begin(),
                                  products.end());

        HandleWeightMap hwm;
        float weight = _cur_rule->get_weight();
        for(Handle& h: derived_rhandles)  hwm[h] = weight;
        _fcstat.add_partial_grounding(_cur_source, rule->get_handle(),
                                      hwm);

        _fcstat.add_inference_record(
                _cur_source, HandleSeq(products.begin(), products.end()));
    }

}

/**
 * Specialized stepper for bio project
 * Does one step forward chaining applying all rules
 *
 * @param fcb a concrete implementation of of ForwardChainerCallBack class
 */
/* needs to be refactored without FCCallBack class
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
*/

void ForwardChainer::do_chain(void)
{
    //Relex2Logic uses this.TODO make a separate class
    //to handle this robustly.
    if(_potential_sources.empty())
    {
        apply_all_rules(_search_focus_Set);
        return;
    }

    auto max_iter = _configReader.get_maximum_iterations();

    while (_iteration < max_iter /*OR other termination criteria*/) {
/* old bio

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

*/
        _log->debug("Iteration %d", _iteration);
         do_step();
        _iteration++;
    }

    _log->debug("[ForwardChainer] finished forwarch chaining.");
}

/* need to refactor without FCCallBack class
void ForwardChainer::do_chain_bio(ForwardChainerCallBack& fcb,
                              Handle hsource)
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

    while (_iteration < max_iter ) { // OR other termination criteria) {
        _log->info("Iteration %d", _iteration);

        do_step_bio(fcb);

        //! Choose next source.
        _log->info("[ForwardChainer] setting next source");
        _fcmem.set_source(fcb.choose_next_source(_fcmem));

        _iteration++;
    }

    _log->info("[ForwardChainer] finished do_chain.");
}

*/

/**
 * Applies all rules in the rule base.
 *
 * @param search_focus_set flag for searching focus set.
 */
void ForwardChainer::apply_all_rules(bool search_focus_set /*= false*/)
{
    for (Rule* rule : _rules) {
        _cur_rule = rule;
        HandleSeq hs = apply_rule(rule->get_handle(), search_focus_set);

        //Update
       _fcstat.add_inference_record(Handle::UNDEFINED,hs);
        update_potential_sources(hs);
    }

}

HandleSeq ForwardChainer::get_chaining_result()
{
    return _fcstat.get_all_inferences();
}

/*
HandleSeq ForwardChainer::get_conclusions()
{
    HandleSeq result;

    for (auto h : conclusions) {
        result.push_back(h);
    }

    return result;
}
*/

Rule* ForwardChainer::choose_rule(Handle hsource, bool subatom_match)
{
    std::map<Rule*, float> rule_weight;
    for (Rule* r : _rules)
        rule_weight[r] = r->get_weight();

    _log->debug("[ForwardChainer] %d rules to be searched",rule_weight.size());

    //Select a rule among the admissible rules in the rule-base via stochastic
    //selection,based on the weights of the rules in the current context.
    Rule* rule = nullptr;

    auto is_matched = [&](const Rule* rule) {
        if (_fcstat.has_partial_grounding(_cur_source)) {
            auto pgmap = _fcstat.get_rule_pg_map(hsource);
            auto it = pgmap.find(rule->get_handle());
            if (it != pgmap.end())
            return true;
        }
        return false;
    };

    std::string match_type = subatom_match ? "sub-atom-unifying" : "unifying";

    _log->debug("[ForwardChainer]%s", match_type.c_str());


    while (not rule_weight.empty()) {
        Rule *temp = _rec.tournament_select(rule_weight);
        bool unified = false;

        if (is_matched(temp)) {
            _log->debug("[ForwardChainer]Found previous matching by %s",
                        match_type.c_str());

            rule = temp;
            break;
        }

        if (subatom_match) {
            if (subatom_unify(hsource, temp)) {
                rule = temp;
                unified = true;
            }
        }

        else {
            HandleSeq hs = temp->get_implicant_seq();
            for (Handle target : hs) {
                if (unify(hsource, target, temp)) {
                    rule = temp;
                    unified = true;
                    break;
                }
            }
        }

        if(unified) break;

        rule_weight.erase(temp);
    }


    if(nullptr != rule)
        _log->debug("[ForwardChainer] Selected rule is %s",
                            (rule->get_handle())->toShortString().c_str());
    else
       _log->debug("[ForwardChainer] No match found.");

    return rule;
};

Handle ForwardChainer::choose_next_source()
{

    URECommons urec(_as);
    map<Handle, float> tournament_elem;

    switch (_ts_mode) {
    case TV_FITNESS_BASED:
        for (Handle s : _potential_sources)
            tournament_elem[s] = urec.tv_fitness(s);
        break;

    case STI_BASED:
        for (Handle s : _potential_sources)
            tournament_elem[s] = s->getSTI();
        break;

    default:
        throw RuntimeException(TRACE_INFO, "Unknown source selection mode.");
        break;
    }

    Handle hchosen = Handle::UNDEFINED;

    //!Prioritize new source selection.
    for (size_t i = 0; i < tournament_elem.size(); i++) {
        Handle hselected = urec.tournament_select(tournament_elem);
        bool selected_before = (boost::find(_selected_sources, hselected)
                != _selected_sources.end());

        if (selected_before) {
            continue;
        } else {
            hchosen = hselected;
            _selected_sources.push_back(hchosen);
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

    //Check for fully grounded outputs returned by derive_rules.
    if (not contains_atomtype(rhandle, VARIABLE_NODE)) {
        //Subatomic matching may have created a non existing implicant
        //atom and if the implicant doesn't exist, nor should the implicand.
        Handle implicant = BindLinkCast(rhandle)->get_body();
        HandleSeq hs;
        if (implicant->getType() == AND_LINK or implicant->getType() == OR_LINK)
            hs = LinkCast(implicant)->getOutgoingSet();
        else
            hs.push_back(implicant);
        //Actual checking here.
        for (Handle& h : hs) {
            if (_as.get_atom(h) == Handle::UNDEFINED or (search_in_focus_set
                    and find(_focus_set.begin(), _focus_set.end(), h) == _focus_set.end())) {
                return {};
            }
        }

        Instantiator inst(&_as);
        Handle houtput = LinkCast(rhandle)->getOutgoingSet().back();
        result.push_back(inst.instantiate(houtput, { }));
    }

    else {
        if (search_in_focus_set) {
            //This restricts PM to look only in the focus set
            AtomSpace focus_set_as;

            //Add focus set atoms to focus_set atomspace
            for (Handle h : _focus_set)
                focus_set_as.add_atom(h);

            //Add source atoms to focus_set atomspace
            for (Handle h : _potential_sources)
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

            _log->debug("Applying rule in focus set %s ",
                        (rhcpy->toShortString()).c_str());

            bl->imply(fs_pmcb, false);

            result = fs_pmcb.get_result_list();

            _log->debug(
                    "Result is %s ",
                    ((_as.add_link(SET_LINK, result))->toShortString()).c_str());

        }
        //Search the whole atomspace
        else {
            AtomSpace derived_rule_as(&_as);

            Handle rhcpy = derived_rule_as.add_atom(rhandle);

            _log->debug("Applying rule on atomspace %s ",
                        (rhcpy->toShortString()).c_str());

            Handle h = bindlink(&derived_rule_as, rhcpy);

            _log->debug("Result is %s ", (h->toShortString()).c_str());

            result = derived_rule_as.get_outgoing(h);
        }
    }

    //add the results back to main atomspace
    for (Handle h : result)
        _as.add_atom(h);

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
                                               const Rule* rule)
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

    bl->imply(gcb, false);

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
HandleSeq ForwardChainer::derive_rules(Handle source, const Rule* rule,
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
UnorderedHandleSet ForwardChainer::get_subatoms(const Rule *rule)
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
 * with their groundings.In case of fully grounded rules,only
 * the output atoms will be added to the list returned.
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

    //Create the BindLink/Rule by substituting vars with groundings
    for (auto& vgmap : filtered_vgmap_list) {
        Handle himplicand = Substitutor::substitute(blptr->get_implicand(),
                                                    vgmap);
        Handle himplicant = Substitutor::substitute(blptr->get_body(), vgmap);

        //Assuming himplicant's set of variables are superset for himplicand's,
        //generate varlist from himplicant.
        Handle hvarlist = as.add_atom(
                gen_sub_varlist(himplicant,
                                LinkCast(hrule)->getOutgoingSet()[0]));
        Handle hderived_rule = as.add_atom(createBindLink(HandleSeq {
            hvarlist, himplicant, himplicand}));
        derived_rules.push_back(hderived_rule);
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
bool ForwardChainer::unify(Handle source, Handle target, const Rule* rule)
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
bool ForwardChainer::subatom_unify(Handle source, const Rule* rule)
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

void ForwardChainer::update_potential_sources(HandleSeq input)
{
    for (Handle i : input) {
        if (boost::find(_potential_sources, i) == _potential_sources.end())
            _potential_sources.push_back(i);
    }
}
