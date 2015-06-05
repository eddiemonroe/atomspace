#include <opencog/atomutils/AtomUtils.h>
#include <opencog/atoms/bind/BindLink.h>
#include <opencog/query/BindLinkAPI.h>

#include <opencog/query/DefaultImplicator.h>
#include "EMForwardChainerCB.h"
#include "EMForwardChainer.h"



using namespace opencog;


EMForwardChainer::EMForwardChainer(AtomSpace * as, string conf_path) :
        as(as), ure_commons(as),
        cpolicy(JsonicControlPolicyParamLoader(as,conf_path))
{
    //printf("Init EMForwardChainer\n");
    cpolicy.load_config();
    fccb = default_fccb = new EMForwardChainerCB(this);

    //_fcmem.set_search_in_af(cpolicy->get_attention_alloc());
    //_fcmem.set_rules(cpolicy->get_rules());
    //_fcmem.set_cur_rule(nullptr);

    all_rules = cpolicy.get_rules();

    // Provide a logger
    string log_level = cpolicy.get_log_level();
    setLogger(opencog::Logger("forward_chainer.log", Logger::getLevelFromString(log_level), true));
    logger.info("Log level set to " + log_level);
}


EMForwardChainer::~EMForwardChainer()
{
    if (default_fccb) {
        delete default_fccb;
    }
}

//void EMForwardChainer::init()
//{
//    //cpolicy and rules might not be needed here, but rather in the CB class
//    cpolicy = new JsonicControlPolicyParamLoader(as, _conf_path);
//    cpolicy->load_config();
//    //_fcmem.set_search_in_af(cpolicy->get_attention_alloc());
//    //_fcmem.set_rules(cpolicy->get_rules());
//    //_fcmem.set_cur_rule(nullptr);
//    //rules = cpolicy->get_rules();
//
//    // Provide a logger
//    //_log = NULL;
//    string log_level = cpolicy->get_log_level();
//    setLogger(new opencog::Logger("forward_chainer.log", Logger::getLevelFromString(log_level), true));
//    logger.info("Log level set to " + log_level);
//}

void EMForwardChainer::setLogger(Logger l)
{
//    if (logger)
//        delete logger;
    logger = l;
}

Logger EMForwardChainer::getLogger()
{
    return logger;
}


set<Handle> EMForwardChainer::do_chain(Handle original_source,
                                EMForwardChainerCallBackBase* new_fccb)
{
    logger.debug("\n\n\n");
    logger.debug("Entering EMForwardChainer::do_chain\n");
    conclusions.empty();

    this->original_source = original_source;
    if (original_source==Handle::UNDEFINED) {
        //do_pm();
        logger.info("Initial source sent to do_chain() is empty. Handling of this not implemented yet.");
        return conclusions;
    }

    if (new_fccb) {
        fccb = new_fccb;
        fccb->set_forwardchainer(this);
    }

    // Should the chainer handle a variable fulfillment query?
    // If so should it just be handed off to the pattern matcher
    /*// Variable fulfillment query.
    UnorderedHandleSet var_nodes = get_outgoing_nodes(original_source,
                                                      { VARIABLE_NODE });
    if (not var_nodes.empty())
        return do_pm(original_source, var_nodes, fcb);
    */
    auto max_iter = cpolicy.get_max_iter();
    //_fcmem.set_source(original_source); //set initial source
    potential_sources.insert(original_source);

    logger.debug("max_iter: %i", max_iter);
    logger.debug("Original source:\n" + original_source->toShortString());

    //current_source = original_source;

    while (iteration < max_iter /*OR other termination criteria*/) {
        logger.info("Iteration %d", iteration);

        // probably should cahnge this -- change stopping criteria
        if (not step())
            break;

//        //! Choose next source.
//        logger.info("[ForwardChainer] setting next source");
//        //_fcmem.set_source(fcb.choose_next_source(_fcmem));:

        iteration++;
    }

    logger.info("[ForwardChainer] finished do_chain.\n=======================================");

    return conclusions;
}





/**
 * Does one step forward chaining
 * @return false if there is not target to explore
 */
bool EMForwardChainer::step()
{
    logger.fine("EMFowardChainer::step()");

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

    current_source = original_source;

    logger.fine("[ForwardChainer] Current source:\n %s",
               current_source->toShortString().c_str());

    //let's start with just iterating through all the rules
    vector<Rule*> chosen_rules = fccb->choose_rules(current_source);
    logger.fine("%i rules chosen for source", chosen_rules.size());

    //for now we are just handling case of source being a single node
    if (!NodeCast(current_source)) {
        logger.info("Link found for source, not implemented yet.");
        return false;
    }

    for (Rule* rule : chosen_rules) {
        // I think below will go into EMForwardChainerCB::apply_rule()
        logger.debug("============================================================================");
        logger.debug("Current rule: %s",rule->get_name().c_str());
        logger.fine(rule->get_handle()->toShortString().c_str());

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
        for (Handle current_varnode :  LinkCast(var_listlink)->getOutgoingSet()) {
            logger.debug("ground rule variable: %s",current_varnode->toShortString().c_str());

            // Could handle special case here for abduction (and probably others) to only substitute
            // for one of Variable A or Variable B, since they are equivalent. Oh wait, that's
            // probably not true because the resulting Inheritance could go both ways.
            // Though are AndLinks ordered? I noticed in the order for the abduction implicant
            // AndLink was reversed from the rule in scheme file definition in debug output.

            map<Handle,Handle> replacement_map = map<Handle,Handle>();
            replacement_map[current_varnode] = current_source;
            //this is actually the grounded implication part of the rule (w/out the variables)
            //had problems gou
            // Handle grounded_rule = ure_commons.change_node_types(implication_link,replacement_map);
            Handle grounded_implicant = ure_commons.change_node_types(implicant,replacement_map);
            Handle grounded_implicand = ure_commons.change_node_types(implicand,replacement_map);
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

            HandleSeq bl_contents = {varlist,grounded_implicant,grounded_implicand};

//            Handle hbindlink = ure_commons.create_bindlink_from_implicationlink(grounded_rule);
            //BindLink bl(bl_contents);
            BindLinkPtr bl_p = createBindLink(bl_contents);
            //Handle bl_h = Handle(*LinkCast(bl));
            Handle bl_h = Handle(bl_p);

            logger.fine("The grounded bindlink:\n%s",bl_h->toShortString().c_str());


            //BindLinkPtr pln_bindlink(BindLinkCast(hbindlink));

            //TODO: should use pln_bindlink, but it's not working (returns empty set)
            HandleSeq rule_conclusions = LinkCast(bindlink(as,bl_h))->getOutgoingSet();


//            BindLinkPtr bindlink(BindLinkCast(grounded_rule));
//            logger.fine("The grounded bindlink:\n%s",pln_bindlink->toShortString().c_str());
//            DefaultImplicator implicator(as);
//            implicator.implicand = pln_bindlink->get_implicand();
//            pln_bindlink->imply(implicator);
//
//            vector<Handle> concl = implicator.result_list;
//            set<Handle> conclutions(concl.begin(), concl.end());
            logger.debug("----------------------------------------------");
            if (rule_conclusions.size() > 0) {
                string conclStr = string(NodeCast(current_varnode)->getName().c_str()) + " Conclusions:\n";
                //logger.debug("Conclusions:");
                for (auto conclusion : rule_conclusions) {
                    //logger.debug(conclusion->toShortString().c_str());
                    conclStr = conclStr + "\n" + conclusion->toShortString().c_str();
                }
                logger.debug(conclStr);

                //add the particular rule's conclusions to the full list of conclusions
                std::copy(rule_conclusions.begin(), rule_conclusions.end(),
                          std::inserter(conclusions, conclusions.end()));
            }
            else {
                logger.debug("%s Conclusions: None",NodeCast(current_varnode)->getName().c_str());
            }
        }

    }


//    set<Rule*> chosen_rules = choose_rules();
//
//    if (chosen_rules.size()>0) {
//        apply_rules(chosen_rules);
//    }


//    printf("Rules:\n");
//    for (auto rule : rules) {
//        printf("%s\n",rule->get_handle()->toShortString().c_str());
//    }


    return true;

////    // Add more premise to hcurrent_source by pattern matching.
////    logger.info("[ForwardChainer] Choose additional premises:");
////    HandleSeq input = fccb.choose_premises(_fcmem);
////    logger.debug("New premises to add:");
////    for (Handle h : input) {
////        if (not _fcmem.isin_premise_list(h))
////            logger.info("%s \n", h->toShortString().c_str());
////    }
////    logger.debug("------ end additional premise list -------------");
////
////    _fcmem.update_premise_list(input);
////
////    // Choose the best rule to apply.
////    vector<Rule*> rules = fccb.choose_rules(_fcmem);
////    map<Rule*, float> rule_weight;
////
////    for (Rule* r : rules) {
////        logger.info("[ForwardChainer] Matching rule %s", r->get_name().c_str());
////        rule_weight[r] = r->get_cost();
////    }
////
////    if (rules.size()==0) {
////        logger.debug("No matching rules found.");
////    }
////
////    auto r = _rec.tournament_select(rule_weight);
////    //! If no rules matches the pattern of the source, choose
////    //! another source if there is, else end forward chaining.
////    if (not r) {
////        if (rules.size()>0) {
////            logger.debug("No rule returned from _rec.tournament_select(rule_weight");
////        }
////        auto new_source = fccb.choose_next_source(_fcmem);
////        if (new_source == Handle::UNDEFINED) {
////            logger.info(
////                    "[ForwardChainer] No chosen rule and no more target to choose.Aborting forward chaining.");
////            return false;
////        } else {
////            logger.info(
////                    "[ForwardChainer] No matching rule,attempting with another source:\n%s.",
////                    new_source->toShortString().c_str());
////            //set source and try another step
////            _fcmem.set_source(new_source);
////            return step(fccb);
////        }
////    }
////
////    _fcmem.set_cur_rule(r);
////
////    //! Apply rule.
////    logger.info("[ForwardChainer] Applying chosen rule %s",
////               r->get_name().c_str());
////    HandleSeq product = fccb.apply_rule(_fcmem);
//////    logger.debug("rule application product:");
//////    logger.debug(product);
////
////    logger.info("[ForwardChainer] Results of rule application");
////    for (auto p : product)
////        logger.info("%s", p->toShortString().c_str());
////
////    logger.info("[ForwardChainer] adding inference to history");
////    _fcmem.add_rules_product(_iteration, product);
////
////    logger.info(
////            "[ForwardChainer] updating premise list with the inference made");
////    _fcmem.update_premise_list(product);
//
//    return true;
}

//vector<Rule*> EMForwardChainer::choose_rule() {
//    return fccb->choose_rule();
//}

//Rule* EMForwardChainer::choose_rule() {
//
//}

Handle EMForwardChainer::choose_source() {
    return fccb->choose_source();
}

//HandleSeq EMForwardChainer::apply_rules(vector<Rule*> rules) {
//    return fccb->apply_rules(rules);
//}

HandleSeq EMForwardChainer::get_chaining_result() {
    HandleSeq result;

    for (auto h : conclusions) {
        result.push_back(h);
    }

    return result;
//    HandleSeq FCMemory::get_result()
//    {
//        HandleSeq result;
//        for (Inference i : _inf_history)
//            result.insert(result.end(), i.inf_product.begin(), i.inf_product.end());
//        return result;
//    }
}

AtomSpace* EMForwardChainer::get_atomspace() {
    return as;
}

Handle& EMForwardChainer::get_current_source() {
    return current_source;
}



