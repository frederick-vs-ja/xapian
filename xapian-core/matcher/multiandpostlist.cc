/** @file
 * @brief N-way AND postlist
 */
/* Copyright (C) 2007,2009,2011,2012,2015,2017 Olly Betts
 * Copyright (C) 2009 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>

#include "multiandpostlist.h"
#include "omassert.h"
#include "debuglog.h"

using namespace std;

void
MultiAndPostList::allocate_plist_and_max_wt()
{
    plist = new PostList * [n_kids];
    try {
	max_wt = new double [n_kids]();
    } catch (...) {
	delete [] plist;
	plist = NULL;
	throw;
    }
}

MultiAndPostList::~MultiAndPostList()
{
    if (plist) {
	for (size_t i = 0; i < n_kids; ++i) {
	    delete plist[i];
	}
	delete [] plist;
    }
    delete [] max_wt;
}

Xapian::doccount
MultiAndPostList::get_termfreq() const
{
    LOGCALL(MATCH, Xapian::doccount, "MultiAndPostList::get_termfreq", NO_ARGS);
    // We shortcut an empty shard and avoid creating a postlist tree for it.
    Assert(db_size);
    // We calculate the estimate assuming independence.  With this assumption,
    // the estimate is the product of the estimates for the sub-postlists
    // divided by db_size (n_kids - 1) times.
    double result = plist[0]->get_termfreq();
    for (size_t i = 1; i < n_kids; ++i) {
	result = (result * plist[i]->get_termfreq()) / db_size;
    }
    return static_cast<Xapian::doccount>(result + 0.5);
}

TermFreqs
MultiAndPostList::estimate_termfreqs(
	const Xapian::Weight::Internal& stats) const
{
    LOGCALL(MATCH, TermFreqs, "MultiAndPostList::estimate_termfreqs", stats);
    // We calculate the estimate assuming independence.  With this assumption,
    // the estimate is the product of the estimates for the sub-postlists
    // divided by db_size (n_kids - 1) times.
    TermFreqs freqs(plist[0]->estimate_termfreqs(stats));

    double freqest = double(freqs.termfreq);
    double relfreqest = double(freqs.reltermfreq);
    double collfreqest = double(freqs.collfreq);

    // Our caller should have ensured this.
    Assert(stats.collection_size);

    for (size_t i = 1; i < n_kids; ++i) {
	freqs = plist[i]->estimate_termfreqs(stats);

	// If the collection is empty, freqest should be 0 already, so leave
	// it alone.
	freqest = (freqest * freqs.termfreq) / stats.collection_size;
	if (usual(stats.total_length != 0)) {
	    collfreqest = (collfreqest * freqs.collfreq) / stats.total_length;
	}

	// If the rset is empty, relfreqest should be 0 already, so leave
	// it alone.
	if (stats.rset_size != 0)
	    relfreqest = (relfreqest * freqs.reltermfreq) / stats.rset_size;
    }

    RETURN(TermFreqs(static_cast<Xapian::doccount>(freqest + 0.5),
		     static_cast<Xapian::doccount>(relfreqest + 0.5),
		     static_cast<Xapian::termcount>(collfreqest + 0.5)));
}

Xapian::docid
MultiAndPostList::get_docid() const
{
    return did;
}

double
MultiAndPostList::get_weight(Xapian::termcount doclen,
			     Xapian::termcount unique_terms,
			     Xapian::termcount wdfdocmax) const
{
    Assert(did);
    double result = 0;
    for (size_t i = 0; i < n_kids; ++i) {
	result += plist[i]->get_weight(doclen, unique_terms, wdfdocmax);
    }
    return result;
}

bool
MultiAndPostList::at_end() const
{
    return (did == 0);
}

double
MultiAndPostList::recalc_maxweight()
{
    max_total = 0.0;
    for (size_t i = 0; i < n_kids; ++i) {
	double new_max = plist[i]->recalc_maxweight();
	max_wt[i] = new_max;
	max_total += new_max;
    }
    return max_total;
}

PostList *
MultiAndPostList::find_next_match(double w_min)
{
advanced_plist0:
    if (plist[0]->at_end()) {
	did = 0;
	return NULL;
    }
    did = plist[0]->get_docid();
    for (size_t i = 1; i < n_kids; ++i) {
	bool valid;
	check_helper(i, did, w_min, valid);
	if (!valid) {
	    next_helper(0, w_min);
	    goto advanced_plist0;
	}
	if (plist[i]->at_end()) {
	    did = 0;
	    return NULL;
	}
	Xapian::docid new_did = plist[i]->get_docid();
	if (new_did != did) {
	    skip_to_helper(0, new_did, w_min);
	    goto advanced_plist0;
	}
    }
    return NULL;
}

PostList *
MultiAndPostList::next(double w_min)
{
    next_helper(0, w_min);
    return find_next_match(w_min);
}

PostList *
MultiAndPostList::skip_to(Xapian::docid did_min, double w_min)
{
    skip_to_helper(0, did_min, w_min);
    return find_next_match(w_min);
}

std::string
MultiAndPostList::get_description() const
{
    string desc("(");
    desc += plist[0]->get_description();
    for (size_t i = 1; i < n_kids; ++i) {
	desc += " AND ";
	desc += plist[i]->get_description();
    }
    desc += ')';
    return desc;
}

Xapian::termcount
MultiAndPostList::get_wdf() const
{
    Xapian::termcount totwdf = 0;
    for (size_t i = 0; i < n_kids; ++i) {
	totwdf += plist[i]->get_wdf();
    }
    return totwdf;
}

Xapian::termcount
MultiAndPostList::count_matching_subqs() const
{
    Xapian::termcount total = 0;
    for (size_t i = 0; i < n_kids; ++i) {
	total += plist[i]->count_matching_subqs();
    }
    return total;
}

void
MultiAndPostList::gather_position_lists(OrPositionList* orposlist)
{
    for (size_t i = 0; i < n_kids; ++i) {
	plist[i]->gather_position_lists(orposlist);
    }
}
