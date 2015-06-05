/*
 * InferenceSCM.h
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

#ifndef _OPENCOG_INFERENCE_SCM_H
#define _OPENCOG_INFERENCE_SCM_H

#include <opencog/atomspace/Handle.h>

namespace opencog {

class InferenceSCM
{
private:
	static void* init_in_guile(void*);
	static void init_in_module(void*);

	void init(void);

	
// Eddie version

	Handle do_forward_chaining_em(Handle source,
		const std::string& conf_path);
	Handle do_forward_chaining_em_default_control(Handle source);

// repo version
	// Run Forward Chaining on source h and rule-based system rbs
	Handle do_forward_chaining(Handle h, Handle rbs);
	/**
	 * @return a handle to a ListLink  of ListLinks holding a variable followed by all grounding nodes.
	 */
	Handle do_backward_chaining(Handle h);
public:
	InferenceSCM();
};

} /*end of namespace opencog*/

extern "C" {
void opencog_ruleengine_init(void);
};

#endif /* _OPENCOG_INFERENCE_SCM_H */
