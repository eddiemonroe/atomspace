/*
 * opencog/atomspace/SimpleTruthValue.cc
 *
 * Copyright (C) 2002-2007 Novamente LLC
 * All Rights Reserved
 *
 * Written by Welter Silva <welter@vettalabs.com>
 *            Guilherme Lamacie

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
 *
 */

#include <math.h>
#include <typeinfo>

#include <opencog/util/platform.h>
#include <opencog/util/exceptions.h>

#include "SimpleTruthValue.h"

//#define DPRINTF printf
#define DPRINTF(...)

using namespace opencog;

#define CVAL  0.2f

SimpleTruthValue::SimpleTruthValue(strength_t m, count_t c)
{
    mean = m;
    count = c;
}

SimpleTruthValue::SimpleTruthValue(const TruthValue& source)
{
    mean = source.getMean();
    count = source.getCount();
}
SimpleTruthValue::SimpleTruthValue(SimpleTruthValue const& source)
{
    mean = source.mean;
    count = source.count;
}

strength_t SimpleTruthValue::getMean() const
{
    return mean;
}

count_t SimpleTruthValue::getCount() const
{
    return count;
}

confidence_t SimpleTruthValue::getConfidence() const
{
    return countToConfidence(count);
}

// This is the merge formula appropriate for PLN.
TruthValuePtr SimpleTruthValue::merge(TruthValuePtr other,
                                      const MergeCtrl& mc) const
{
    switch(mc.tv_formula) {
    case MergeCtrl::TVFormula::HIGHER_CONFIDENCE:
        return higher_confidence_merge(other);
    case MergeCtrl::TVFormula::PLN_BOOK_REVISION:
    {
        // Based on Section 5.10.2(A heuristic revision rule for STV)
        // of the PLN book
        if (other->getType() != SIMPLE_TRUTH_VALUE)
            throw RuntimeException(TRACE_INFO,
                                   "Don't know how to merge %s into a "
                                   "SimpleTruthValue using the default style",
                                   typeid(*other).name());
        auto count2 = other->getCount();
        auto count_new = count + count2 - std::min(count, count2) * CVAL;
        auto mean_new = (mean * count + other->getMean() * count2)
            / (count + count2);
        return std::make_shared<SimpleTruthValue>(mean_new, count_new);
    }
    default:
        throw RuntimeException(TRACE_INFO,
                               "SimpleTruthValue::merge: case not implemented");
        return nullptr;
    }
}

std::string SimpleTruthValue::toString() const
{
    char buf[1024];
    sprintf(buf, "(stv %f %f)",
            static_cast<float>(getMean()),
            static_cast<float>(getConfidence()));
    return buf;
}

bool SimpleTruthValue::operator==(const TruthValue& rhs) const
{
    const SimpleTruthValue *stv = dynamic_cast<const SimpleTruthValue *>(&rhs);
    if (NULL == stv) return false;

#define FLOAT_ACCEPTABLE_MEAN_ERROR 0.000001
    if (FLOAT_ACCEPTABLE_MEAN_ERROR < fabs(mean - stv->mean)) return false;

// Converting from confidence to count and back again using single-precision
// float is a real accuracy killer.  In particular, 2/802 = 0.002494 but
// converting back gives 800*0.002494/(1.0-0.002494) = 2.000188 and so
// comparison tests can only be accurate to about 0.000188 or
// thereabouts.
#define FLOAT_ACCEPTABLE_COUNT_ERROR 0.0002

    if (FLOAT_ACCEPTABLE_COUNT_ERROR < fabs(1.0 - (stv->count/count))) return false;
    return true;
}

TruthValueType SimpleTruthValue::getType() const
{
    return SIMPLE_TRUTH_VALUE;
}

count_t SimpleTruthValue::confidenceToCount(confidence_t cf)
{
    // There are not quite 16 digits in double precision
    // not quite 7 in single-precision float
    cf = std::min(cf, 0.9999998f);
    return static_cast<count_t>(DEFAULT_K * cf / (1.0f - cf));
}

confidence_t SimpleTruthValue::countToConfidence(count_t cn)
{
    return static_cast<confidence_t>(cn / (cn + DEFAULT_K));
}
