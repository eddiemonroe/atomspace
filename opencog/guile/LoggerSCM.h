/*
 * LoggerSCM.h
 *
 * Copyright (C) 2015 OpenCog Foundation
 *
 * Author: Nil Geisweiller
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

#ifndef _OPENCOG_LOGGER_SCM_H
#define _OPENCOG_LOGGER_SCM_H

#include <opencog/guile/SchemeModule.h>

namespace opencog {

/**
 * Expose the Logger singleton to Scheme
 */

class LoggerSCM : public ModuleWrap
{
protected:
	virtual void init();

public:
	LoggerSCM();
};

void do_logger_set_level(const std::string& level);
const std::string& do_logger_get_level();
void do_logger_set_filename(const std::string& filename);
const std::string& do_logger_get_filename();
void do_logger_set_stdout(bool);
void do_logger_error(const std::string& msg);
void do_logger_warn(const std::string& msg);
void do_logger_info(const std::string& msg);
void do_logger_debug(const std::string& msg);
void do_logger_fine(const std::string& msg);
	
} /*end of namespace opencog*/

extern "C" {
void opencog_logger_init(void);
};

#endif /* _OPENCOG_INFERENCE_SCM_H */
