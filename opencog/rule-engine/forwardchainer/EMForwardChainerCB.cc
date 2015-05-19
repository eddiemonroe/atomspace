#include <opencog/rule-engine/Rule.h>
#include <opencog/atoms/bind/BindLink.h>

#include "EMForwardChainerCB.h"

using namespace opencog;

EMForwardChainerCB::EMForwardChainerCB(EMForwardChainer* fc) : fc(fc)
{
    as = fc->get_atomspace();
//    as = as;
//    fcim_ = new ForwardChainInputMatchCB(as);
//    fcpm_ = new ForwardChainPatternMatchCB(as);
//    ts_mode_ = ts_mode;

//    _log = opencog::Logger("forward_chainer.log", Logger::FINE, true);
}

EMForwardChainerCB::~EMForwardChainerCB()
{

}

HandleSeq EMForwardChainerCB::choose_premises() {
    HandleSeq h;
    return h;
};
Handle EMForwardChainerCB::choose_source() {
    //for now we are just using one source
    return fc->get_current_source();

                                                                                                                                                                                                                                                                                                                                                }
HandleSeq EMForwardChainerCB::apply_rule(Rule& rule) {
    UnorderedHandleSet h;

    BindLinkPtr bindlink(BindLinkCast(rule.get_handle()));
    DefaultImplicator implicator(fc->get_atomspace());
    implicator.implicand = bindlink->get_implicand();


    bindlink->imply(implicator);
    //HandleSeq product = fcpm_->get_products();
    HandleSeq conclusions = implicator.result_list;


    return conclusions;

}



////not using currently?
//UnorderedHandleSet EMForwardChainerCB::apply_rules(vector<Rule*> rules) {
//    UnorderedHandleSet rule_conclusions;
//    for (Rule* rule : rules) {
//        rule_conclusions.insert(apply_rule(*rule));
//    }
//    return rule_conclusions;
//}




vector<Rule *> EMForwardChainerCB::choose_rules(Handle source) {

    //for now just return all rules
    return fc->all_rules;

//
//    HandleSeq chosen_bindlinks;
//
//    if (LinkCast(source)) {
//        AtomSpace rule_atomspace;
//        Handle source_cpy = rule_atomspace.addAtom(source);
//
//        // Copy rules to the temporary atomspace.
//        vector<Rule *> rules = fcmem.get_rules();
//        for (Rule *r : rules) {
//            rule_atomspace.addAtom(r->get_handle());
//        }
//
//        _log.debug("All rules:");
//        for (Rule *r: rules) {
//            _log.debug(r->get_name());
//        }
//
//        // Create bindlink with source as an implicant.
//        URECommons urec(&rule_atomspace);
//        Handle copy = urec.replace_nodes_with_varnode(source_cpy, NODE);
//        Handle bind_link = urec.create_bindLink(copy, false);
//
//        // Pattern match.
//        BindLinkPtr bl(BindLinkCast(bind_link));
//        DefaultImplicator imp(&rule_atomspace);
//        imp.implicand = bl->get_implicand();
//        bl->imply(imp);
//
//        // Get matched bindLinks.
//        HandleSeq matches = imp.result_list;
//        if (matches.empty()) {
//            _log.debug(
//                    "No matching BindLink was found. Returning empty vector");
//            return vector<Rule *> {};
//        }
//
//        UnorderedHandleSet bindlinks;
//        for (Handle hm : matches) {
//            //get all BindLinks whose part of their premise matches with hm
//            HandleSeq hs = get_rootlinks(hm, &rule_atomspace, BIND_LINK);
//            bindlinks.insert(hs.cbegin(), hs.cend());
//        }
//
//        // Copy handles to main atomspace.
//        for (Handle h : bindlinks) {
//            chosen_bindlinks.push_back(as_->addAtom(h));
//        }
//    } else {
//        // Try to find specialized rules that contain the source node.
//        _log.debug("source is not a link. Try to find specialized rules that contain the source node.");
//        OC_ASSERT(NodeCast(source) != nullptr);
//        chosen_bindlinks = get_rootlinks(source, as_, BIND_LINK);
//    }
//
//    _log.debug("chosen_bindlinks:");
//    //sleep(1);
//
//    for (auto bl : chosen_bindlinks) {
//        _log.debug(bl->toString());
//        //sleep(1);
//
//    }
//
//    // Find the rules containing the bindLink in copied_back.
//    vector<Rule *> matched_rules;
//    for (Rule *r : fcmem.get_rules()) {
//        auto it = find(chosen_bindlinks.begin(), chosen_bindlinks.end(),
//                       r->get_handle()); //xxx not matching
//        if (it != chosen_bindlinks.end()) {
//            matched_rules.push_back(r);
//        }
//    }
//
//    return matched_rules;


}

void EMForwardChainerCB::set_forwardchainer(EMForwardChainer *new_fc) {
    fc = new_fc;
}