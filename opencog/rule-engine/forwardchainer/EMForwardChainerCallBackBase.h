/*
 * ForwardChainerCallBack.h
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

#ifndef EMFORWARDCHAINERCALLBACKBASE_H_
#define EMFORWARDCHAINERCALLBACKBASE_H_

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atomspace/Handle.h>

#include "EMForwardChainer.h"
//#include "EMForwardChainerCB.h"

namespace opencog {

enum em_source_selection_mode {
    EM_TV_FITNESS_BASED, EM_STI_BASED
};

class Rule;
//class FCMemory;
class EMForwardChainer;

class EMForwardChainerCallBackBase {
private:
    AtomSpace* as;
public:
    //EMForwardChainerCallBackBase(EMForwardChainerCB* fc) {}

    EMForwardChainerCallBackBase() {}
    virtual ~EMForwardChainerCallBackBase()
    {
    }
    /**
     * Choose a set of applicable rules from the rule base by selecting
     * rules whose premise structurally matches with the source.
     * @fcmem an object holding the current source/target and other inform
     * ation of the forward chaining instance.
     * @return a set of applicable rules
     */
    virtual std::vector<Rule*> choose_rules(Handle source) = 0;
    /**
     * Choose additional premises for the rule.
     * @fcmem an object holding the current source/target and other inform
     * ation of the forward chaining instance.
     * @return a set of Handles chosen as a result of applying fitness
     * criteria with respect to the current source.
     */
//    virtual HandleSeq choose_premises() = 0;
    /**
     * choose next source from the source list
     * @return a handle to the chosen source from source list
     */
    virtual Handle choose_source() = 0;
    /**
     * apply chosen rule. the default will wrap a custom PM callback class.
     * i.e invokes _pattern_matcher.
     * @return a set of handles created as a result of applying current choosen rule
     */
    virtual HandleSeq apply_rule(Rule& rule) = 0;
//    virtual UnorderedHandleSet apply_rules(vector<Rule*> rules) = 0;


    virtual void set_forwardchainer(EMForwardChainer* fc) = 0;
};

} // ~namespace opencog

#endif /* EMFORWARDCHAINERCALLBACKBASE_H_ */
