/** @file
 * @brief Parser for OpenDocument's meta.xml.
 */
/* Copyright (C) 2006,2009,2010,2011,2013,2015,2020,2022 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "opendocmetaparser.h"

#include "datetime.h"
#include "parseint.h"

using namespace std;

void
OpenDocMetaParser::process_content(const string& content)
{
    switch (field) {
	case KEYWORDS:
	    if (!keywords.empty()) keywords += ' ';
	    keywords += content;
	    break;
	case TITLE:
	    if (!title.empty()) title += ' ';
	    title += content;
	    break;
	case SAMPLE:
	    if (!sample.empty()) sample += ' ';
	    sample += content;
	    break;
	case AUTHOR:
	    if (!author.empty()) author += ' ';
	    author += content;
	    break;
	case CREATED: {
	    // E.g. 2013-03-04T22:57:00
	    created = parse_datetime(content);
	    break;
	}
	case NONE:
	    // Ignore other fields.
	    break;
    }
}

bool
OpenDocMetaParser::opening_tag(const string& tag)
{
    if (tag.size() < 8) return true;
    if (tag[0] == 'd' && tag[1] == 'c') {
	if (tag == "dc:subject") {
	    // dc:subject is "Subject and Keywords":
	    // "Typically, Subject will be expressed as keywords, key phrases
	    // or classification codes that describe a topic of the resource."
	    // OpenOffice uses meta:keywords for keywords - dc:subject
	    // comes from a text field labelled "Subject".  Let's just treat
	    // it as more keywords.
	    field = KEYWORDS;
	} else if (tag == "dc:title") {
	    field = TITLE;
	} else if (tag == "dc:description") {
	    field = SAMPLE;
	} else if (tag == "dc:creator") {
	    field = AUTHOR;
	}
    } else if (tag[0] == 'm') {
	if (tag == "meta:keyword") {
	    // e.g.:
	    // <meta:keywords>
	    // <meta:keyword>information retrieval</meta:keyword>
	    // </meta:keywords>
	    field = KEYWORDS;
	} else if (tag == "meta:creation-date") {
	    field = CREATED;
	} else if (tag == "meta:document-statistic") {
	    // For OpenDocument, the values we want for the page count are to
	    // be found as attributes of the meta:document-statistic tag (which
	    // occurs inside <office:meta> but we don't bother to check that).
	    //
	    // For text documents, we want the meta:page-count attribute.
	    //
	    // For spreadsheets, meta:table-count seems to give the sheet count
	    // (text documents also have meta:table-count so we check for this
	    // after meta:page-count).
	    string value;
	    if (get_attribute("meta:page-count", value) ||
		get_attribute("meta:table-count", value)) {
		unsigned u_pages;
		if (parse_unsigned(value.c_str(), u_pages))
		    pages = int(u_pages);
	    }
	}
    }
    return true;
}

bool
OpenDocMetaParser::closing_tag(const string&)
{
    field = NONE;
    return true;
}
