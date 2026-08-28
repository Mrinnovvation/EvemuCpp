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

#include "eve-server.h"

#include "espserver/ESPHtmlWriter.h"

/* The stock ESP pages were plain tables on a dark background with a menu bar across
 * the top. This keeps that shape without pulling in any external asset, so the page
 * renders standalone in whatever browser blue.os.ShellExecute() happens to open.
 */
static const char* const ESP_PAGE_STYLE =
    "body{background:#1b1d21;color:#d5d8dc;font:13px/1.5 Verdana,Arial,sans-serif;margin:0}"
    "a{color:#7fb2e5}"
    "a:hover{color:#a9cdf2}"
    "#menu{background:#2a2d33;border-bottom:1px solid #3b3f47;padding:8px 16px}"
    "#menu a{margin-right:16px;text-decoration:none;font-weight:bold}"
    "#who{float:right;color:#8b9099;font-weight:normal}"
    "#content{padding:16px}"
    "h1{font-size:18px;margin:0 0 4px}"
    "h2{font-size:15px;margin:20px 0 6px;color:#e8b96b}"
    "table{border-collapse:collapse;margin:8px 0;max-width:100%}"
    "th{background:#2a2d33;text-align:left;padding:5px 10px;border:1px solid #3b3f47;white-space:nowrap}"
    "td{padding:4px 10px;border:1px solid #3b3f47;vertical-align:top}"
    "tr:nth-child(even) td{background:#212429}"
    ".err{color:#e07b7b}"
    ".ok{color:#7bc47b}"
    "form{margin:12px 0}"
    "button{background:#8a2f2f;color:#fff;border:1px solid #b04040;padding:6px 14px;cursor:pointer}";

ESPHtmlWriter::ESPHtmlWriter(const std::string& title, const std::string& userName)
: m_title(title),
  m_userName(userName)
{
    AppendHeader();
}

std::string ESPHtmlWriter::Escape(const std::string& text)
{
    std::string out;
    out.reserve(text.length());

    for (std::string::const_iterator itr = text.begin(); itr != text.end(); ++itr) {
        switch (*itr) {
            case '&':  out += "&amp;";   break;
            case '<':  out += "&lt;";    break;
            case '>':  out += "&gt;";    break;
            case '"':  out += "&quot;";  break;
            case '\'': out += "&#39;";   break;
            default:   out += *itr;      break;
        }
    }

    return out;
}

void ESPHtmlWriter::AppendHeader()
{
    const std::string title = Escape(m_title);

    m_body += "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">";
    m_body += "<title>" + title + " - EVEmu ESP</title>";
    m_body += "<style>";
    m_body += ESP_PAGE_STYLE;
    m_body += "</style></head><body>\n";

    m_body += "<div id=\"menu\">";
    m_body += "<span id=\"who\">" + Escape(m_userName) + "</span>";
    m_body += Link("/", "HOME");
    m_body += Link("/gm/character.py?action=MyCharacters", "MY CHARACTERS");
    m_body += Link("/gm/character.py", "CHARACTERS");
    m_body += Link("/gm/users.py", "USERS");
    m_body += Link("/gd/type.py", "TYPES");
    m_body += "</div>\n";

    m_body += "<div id=\"content\">\n";
    m_body += "<h1>" + title + "</h1>\n";
}

void ESPHtmlWriter::Heading(const std::string& text)
{
    m_body += "<h2>" + Escape(text) + "</h2>\n";
}

void ESPHtmlWriter::Paragraph(const std::string& text)
{
    m_body += "<p>" + Escape(text) + "</p>\n";
}

void ESPHtmlWriter::Error(const std::string& text)
{
    m_body += "<p class=\"err\">" + Escape(text) + "</p>\n";
}

void ESPHtmlWriter::Notice(const std::string& text)
{
    m_body += "<p class=\"ok\">" + Escape(text) + "</p>\n";
}

void ESPHtmlWriter::BeginTable(const std::vector<std::string>& columns)
{
    m_body += "<table><tr>";
    for (size_t i = 0; i < columns.size(); ++i)
        m_body += "<th>" + Escape(columns.at(i)) + "</th>";
    m_body += "</tr>\n";
}

void ESPHtmlWriter::Row(const std::vector<std::string>& cells)
{
    m_body += "<tr>";
    for (size_t i = 0; i < cells.size(); ++i)
        m_body += "<td>" + Escape(cells.at(i)) + "</td>";
    m_body += "</tr>\n";
}

void ESPHtmlWriter::RawRow(const std::vector<std::string>& cells)
{
    m_body += "<tr>";
    for (size_t i = 0; i < cells.size(); ++i)
        m_body += "<td>" + cells.at(i) + "</td>";
    m_body += "</tr>\n";
}

void ESPHtmlWriter::EndTable()
{
    m_body += "</table>\n";
}

void ESPHtmlWriter::DefinitionList(const std::vector<std::string>& namesAndValues)
{
    m_body += "<table>";
    // Trailing odd element (a name with no value) is ignored rather than read past the end.
    for (size_t i = 0; (i + 1) < namesAndValues.size(); i += 2) {
        m_body += "<tr><th>" + Escape(namesAndValues.at(i)) + "</th>";
        m_body += "<td>" + Escape(namesAndValues.at(i + 1)) + "</td></tr>\n";
    }
    m_body += "</table>\n";
}

std::string ESPHtmlWriter::Link(const std::string& href, const std::string& text)
{
    return "<a href=\"" + Escape(href) + "\">" + Escape(text) + "</a>";
}

void ESPHtmlWriter::ConfirmForm(const std::string& action, const std::string& buttonLabel,
                                const std::vector<std::string>& hiddenNamesAndValues)
{
    m_body += "<form method=\"post\" action=\"" + Escape(action) + "\">";
    for (size_t i = 0; (i + 1) < hiddenNamesAndValues.size(); i += 2) {
        m_body += "<input type=\"hidden\" name=\"" + Escape(hiddenNamesAndValues.at(i)) + "\"";
        m_body += " value=\"" + Escape(hiddenNamesAndValues.at(i + 1)) + "\">";
    }
    m_body += "<button type=\"submit\">" + Escape(buttonLabel) + "</button>";
    m_body += "</form>\n";
}

std::string ESPHtmlWriter::Finish()
{
    m_body += "</div>\n</body></html>\n";
    return m_body;
}
