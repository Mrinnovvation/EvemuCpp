/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        EVEmu contributors
*/

#ifndef __ESPHTMLWRITER_H_INCL__
#define __ESPHTMLWRITER_H_INCL__

#include <string>
#include <vector>

#include "eve-common.h"

/**
 * \class ESPHtmlWriter
 *
 * @brief Builds the HTML for an ESP (EVE Server Pages) page.
 *
 * The stock client links straight into these pages from its GM right-click menu
 * (see menusvc.GetFromESP), so the page set and the URL shapes are dictated by the
 * client. This class only deals with presentation; page content comes from ESPPages.
 *
 * Everything that reaches the page from the request or the database goes through
 * Escape(), so callers should pass raw values and let the writer handle quoting.
 */
class ESPHtmlWriter
{
public:
    /**
     * @param[in] title     Page title, shown in the browser tab and as the heading.
     * @param[in] userName  Account name of the logged-in GM, shown in the header bar.
     */
    ESPHtmlWriter(const std::string& title, const std::string& userName);

    /// HTML-escapes a string for use as element text or an attribute value.
    static std::string Escape(const std::string& text);
    /// Percent-encodes a string for use inside a URL query value.
    static std::string UrlEncode(const std::string& text);

    /// Emits a section heading.
    void Heading(const std::string& text);
    /// Emits a paragraph of body text.
    void Paragraph(const std::string& text);
    /// Emits a paragraph styled as an error.
    void Error(const std::string& text);
    /// Emits a paragraph styled as a success/confirmation notice.
    void Notice(const std::string& text);

    /// Opens a table with the given column headings.
    void BeginTable(const std::vector<std::string>& columns);
    /// Emits one row. Cells are escaped; use RawRow() for cells containing markup.
    void Row(const std::vector<std::string>& cells);
    /// Emits one row whose cells are already valid HTML (e.g. built with Link()).
    void RawRow(const std::vector<std::string>& cells);
    /// Closes the table opened by BeginTable().
    void EndTable();

    /// Emits a two-column name/value table from a flat list of alternating pairs.
    void DefinitionList(const std::vector<std::string>& namesAndValues);

    /// Builds an <a href> to an ESP page. Both arguments are escaped.
    static std::string Link(const std::string& href, const std::string& text);

    /// Emits a POST form with a single confirm button and hidden fields.
    void ConfirmForm(const std::string& action, const std::string& buttonLabel,
                     const std::vector<std::string>& hiddenNamesAndValues);

    /// Returns the finished document. Safe to call once; appends the page footer.
    std::string Finish();

private:
    void AppendHeader();

    std::string m_title;
    std::string m_userName;
    std::string m_body;
    size_t m_tableColumnCount;
};

#endif  // __ESPHTMLWRITER_H_INCL__
