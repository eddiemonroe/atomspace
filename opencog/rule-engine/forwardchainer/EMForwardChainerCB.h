//
// Created by eddie on 5/14/15.
//

#ifndef OPENCOG_EMFORWARDCHAINERCB_H
#define OPENCOG_EMFORWARDCHAINERCB_H

#include "EMForwardChainerCallBackBase.h"
#include "ForwardChainInputMatchCB.h"
#include "ForwardChainPatternMatchCB.h"

#include "EMForwardChainer.h"


namespace opencog {

//class FCMemory;
//class Rule;
class EMForwardChainerCB: public virtual EMForwardChainerCallBackBase
{
private:
    EMForwardChainer* fc;
    AtomSpace * as;
    ForwardChainInputMatchCB* fcim_;
    ForwardChainPatternMatchCB* fcpm_;
    HandleSeq get_rootlinks(Handle hsource, AtomSpace* as, Type link_type,
                            bool subclasses = false);
    em_source_selection_mode ts_mode;
    opencog::Logger logger;
public:
//        EMForwardChainerCB(AtomSpace* as, source_selection_mode ts_mode =
//        TV_FITNESS_BASED);

    EMForwardChainerCB(EMForwardChainer* fc);
    virtual ~EMForwardChainerCB();


    void set_logger(Logger h);

    virtual vector<Rule*> choose_rules(Handle source);
    virtual HandleSeq choose_premises();
    virtual Handle choose_source();
    virtual HandleSeq apply_rule(Rule& rule);
//    virtual UnorderedHandleSet apply_rules(vector<Rule*> rules);

    virtual void set_forwardchainer(EMForwardChainer* fc);
};

} // ~namespace opencog


#endif // OPENCOG_EMFORWARDCHAINERCB_H
