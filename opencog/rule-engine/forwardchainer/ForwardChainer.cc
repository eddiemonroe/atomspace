/*
 * ForwardChainer.cc
 *
 * Copyright (C) 2014,2015 Misgana Bayetta
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
#include <opencog/atoms/bind/PatternLink.h>
#include <opencog/atomutils/AtomUtils.h>
#include <opencog/query/DefaultImplicator.h>
#include <opencog/rule-engine/Rule.h>
#include <opencog/atoms/bind/BindLink.h>
#include "ForwardChainer.h"
#include "ForwardChainerCallBack.h"

using namespace opencog;

int MAX_PM_RESULTS = 10000;

ForwardChainer::ForwardChainer(AtomSpace& as, Handle rbs) :
	_as(as), _rec(_as), _rbs(rbs), _configReader(as, rbs), _fcmem(&as)
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

    // Provide a logger
    _log = NULL;
    setLogger(new opencog::Logger("forward_chainer.log", Logger::FINE, true));
    
    //temp code for debugging rules
    _log->debug("All FC Rules:");
    for (Rule* rule : _fcmem.get_rules()) {
        _log->debug(getFormulaName(rule));
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

/**
 * Does one step forward chaining
 *
 * @param fcb a concrete implementation of of ForwardChainerCallBack class 
 */
void ForwardChainer::do_step(ForwardChainerCallBack& fcb)
{

    if (_fcmem.get_cur_source() == Handle::UNDEFINED) {
        _log->info("[ForwardChainer] No current source, step "
                   "forward chaining aborted.");
        return;
    }

    _log->info("[ForwardChainer] Next source %s",
               _fcmem.get_cur_source()->toString().c_str());

    // Choose matching rules whose input matches with the source.
    vector<Rule*> matched_rules = fcb.choose_rules(_fcmem);
    _log->info("[ForwardChainer] Found matching rule(s) (maybe)");

    //! If no rules matches the pattern of the source,
    //! set all rules for candidacy to be selected by the proceeding step.
    //! xxx this decision is maded based on recent discussion.I
    //! it might still face some changes.
    if (matched_rules.empty()) {
        _log->info("[ForwardChainer] No matching rule found. "
                   "Setting all rules as candidates.");
        matched_rules = _fcmem.get_rules();
    }

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

    //!TODO Find/add premises?

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
    }

    _log->info("[ForwardChainer] updating premise list with the "
               "inference made");
    _fcmem.update_potential_sources(product);

    _log->info("[ForwardChainer] adding inference to history");
    _fcmem.add_rules_product(_iteration, product);
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
            rule_conclusions = impl.result_list;


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


void ForwardChainer::do_chain(ForwardChainerCallBack& fcb,
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

    HandleSeq init_sources;
    //Accept set of initial sources wrapped by a SET_LINK
    if(LinkCast(hsource) and hsource->getType() == SET_LINK)
     init_sources = _as.get_outgoing(hsource);
    else
        init_sources.push_back(hsource);
    _fcmem.update_potential_sources(init_sources);

    _fcmem.set_source(fcb.choose_next_source(_fcmem)); //set initial source
    auto max_iter = _configReader.get_maximum_iterations();

    while (_iteration < max_iter /*OR other termination criteria*/) {
        _log->info("Iteration %d", _iteration);

        do_step(fcb);

        //! Choose next source.
        _log->info("[ForwardChainer] setting next source");
        _fcmem.set_source(fcb.choose_next_source(_fcmem));

        _iteration++;
    }

    _log->info("[ForwardChainer] finished do_chain.");
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
 * @param source a variable containing handle passed as an input to the pattern matcher
 * @param var_nodes the VariableNodes in @param hsource
 * @param fcb a forward chainer callback implementation used here only for choosing rules
 * that contain @param hsource in their implicant
 */
void ForwardChainer::do_pm(const Handle& hsource,
                           const UnorderedHandleSet& var_nodes,
                           ForwardChainerCallBack& fcb)
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
    _fcmem.add_rules_product(0, impl.result_list);

    // Delete the AND_LINK and LIST_LINK
    _as.remove_atom(hvar_list);
    _as.remove_atom(hclause);

    //!Additionally, find applicable rules and apply.
    vector<Rule*> rules = fcb.choose_rules(_fcmem);
    for (Rule* rule : rules) {
        BindLinkPtr bl(BindLinkCast(rule->get_handle()));
        DefaultImplicator impl(&_as);
        impl.implicand = bl->get_implicand();
        bl->imply(impl);
        _fcmem.set_cur_rule(rule);
        _fcmem.add_rules_product(0, impl.result_list);
    }
}
/**
 * Invokes pattern matcher using each rule declared in the configuration file.
 */
void ForwardChainer::do_pm()
{
    //! Do pattern matching using the rules declared in the declaration file
    _log->info("Forward chaining on the rule-based system %s "
               "declared in %s", _rbs->toString().c_str());
    vector<Rule*> rules = _fcmem.get_rules();
    for (Rule* rule : rules) {
        _log->info("Applying rule %s on ", rule->get_name().c_str());
        BindLinkPtr bl(BindLinkCast(rule->get_handle()));
        DefaultImplicator impl(&_as);
        impl.implicand = bl->get_implicand();
        bl->imply(impl);
        _fcmem.set_cur_rule(rule);

        _log->info("OUTPUTS");
        for (auto h : impl.result_list)
            _log->info("%s", h->toString().c_str());

        _fcmem.add_rules_product(0, impl.result_list);
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
