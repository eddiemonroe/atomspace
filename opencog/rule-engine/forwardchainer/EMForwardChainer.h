


#ifndef OPENCOG_EMFORWARDCHAINER1_H
#define OPENCOG_EMFORWARDCHAINER1_H

#include <boost/container/flat_set.hpp>

#include <opencog/util/Logger.h>
#include <opencog/rule-engine/JsonicControlPolicyParamLoader.h>
#include <opencog/rule-engine/URECommons.h>
#include "FCMemory.h"

#include "EMForwardChainerCallBackBase.h"

//#include "ForwardChainer.h"

namespace opencog {

//static const char* DEFAULT_CONFIG_PATH = "opencog/reasoning/default_cpolicy.json";
static const char* DEFAULT_CONFIG_PATH = "opencog/reasoning/bio_cpolicy.json";


typedef boost::container::flat_set<Handle> HandleFlatSet;

class EMChainerUTest;
class EMForwardChainerCallBackBase;


class EMForwardChainer  {
friend class EMForwardChainerCallBackBase;

private:
    AtomSpace * as;
    URECommons ure_commons; //utility class
    JsonicControlPolicyParamLoader cpolicy;

    EMForwardChainerCallBackBase *fccb, *default_fccb;


    bool step();

    Handle choose_source();
    vector<Rule*> choose_rules();
    HandleSeq apply_rules(vector<Rule*> rules);

    //void init();
    void add_to_source_list(Handle h);



    Logger logger;
    int iteration = 0;

//    void do_pm();
//    void do_pm(const Handle& hsource, const UnorderedHandleSet& var_nodes,
//               ForwardChainerCallBack& fcb);

protected:
    Handle current_source, original_source;
    HandleFlatSet potential_sources = HandleFlatSet();

public:
    vector<Rule*> all_rules;

    EMForwardChainer(AtomSpace* as, string conf_path=DEFAULT_CONFIG_PATH);
    virtual ~EMForwardChainer();

    void do_chain(Handle original_source=Handle::UNDEFINED,
                  EMForwardChainerCallBackBase* new_fccb=NULL);

    HandleSeq get_chaining_result(void);

    void setLogger(Logger log);
    Logger getLogger(void);

    AtomSpace * get_atomspace();
    Handle& get_current_source();






    };


}

#endif //OPENCOG_EMFORWARDCHAINER1_H


