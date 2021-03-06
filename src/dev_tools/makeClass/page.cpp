/*-------------------------------------------------------------------------------------------
 * qblocks - fast, easily-accessible, fully-decentralized data from blockchains
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 *
 * This program is free software: you may redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version. This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. You should have received a copy of the GNU General
 * Public License along with this program. If not, see http://www.gnu.org/licenses/.
 *-------------------------------------------------------------------------------------------*/
/*
 * This file was generated with makeClass. Edit only those parts of the code inside
 * of 'EXISTING_CODE' tags.
 */
#include <algorithm>
#include "page.h"

namespace qblocks {

//---------------------------------------------------------------------------
IMPLEMENT_NODE(CPage, CBaseNode);

//---------------------------------------------------------------------------
static string_q nextPageChunk(const string_q& fieldIn, const void* dataPtr);
static string_q nextPageChunk_custom(const string_q& fieldIn, const void* dataPtr);

//---------------------------------------------------------------------------
void CPage::Format(ostream& ctx, const string_q& fmtIn, void* dataPtr) const {
    if (!m_showing)
        return;

    // EXISTING_CODE
    // EXISTING_CODE

    string_q fmt = (fmtIn.empty() ? expContext().fmtMap["page_fmt"] : fmtIn);
    if (fmt.empty()) {
        ctx << toJson();
        return;
    }

    // EXISTING_CODE
    // EXISTING_CODE

    while (!fmt.empty())
        ctx << getNextChunk(fmt, nextPageChunk, this);
}

//---------------------------------------------------------------------------
string_q nextPageChunk(const string_q& fieldIn, const void* dataPtr) {
    if (dataPtr)
        return reinterpret_cast<const CPage*>(dataPtr)->getValueByName(fieldIn);

    // EXISTING_CODE
    // EXISTING_CODE

    return fldNotFound(fieldIn);
}

//---------------------------------------------------------------------------
string_q CPage::getValueByName(const string_q& fieldName) const {
    // Give customized code a chance to override first
    string_q ret = nextPageChunk_custom(fieldName, this);
    if (!ret.empty())
        return ret;

    // EXISTING_CODE
    // EXISTING_CODE

    // Return field values
    switch (tolower(fieldName[0])) {
        case 'c':
            if (fieldName % "color") {
                return color;
            }
            break;
        case 'f':
            if (fieldName % "files") {
                return files;
            }
            break;
        case 'h':
            if (fieldName % "has_text") {
                return bool_2_Str(has_text);
            }
            break;
        case 'l':
            if (fieldName % "longName") {
                return longName;
            }
            break;
        case 'm':
            if (fieldName % "menuType") {
                return menuType;
            }
            break;
        case 'n':
            if (fieldName % "no_error") {
                return bool_2_Str(no_error);
            }
            if (fieldName % "no_data") {
                return bool_2_Str(no_data);
            }
            if (fieldName % "no_dt") {
                return bool_2_Str(no_dt);
            }
            if (fieldName % "no_dash") {
                return bool_2_Str(no_dash);
            }
            break;
        case 'p':
            if (fieldName % "properName") {
                return properName;
            }
            if (fieldName % "pageNotes") {
                return pageNotes;
            }
            if (fieldName % "polling") {
                return bool_2_Str(polling);
            }
            break;
        case 's':
            if (fieldName % "sevenName") {
                return sevenName;
            }
            if (fieldName % "subpages" || fieldName % "subpagesCnt") {
                size_t cnt = subpages.size();
                if (endsWith(toLower(fieldName), "cnt"))
                    return uint_2_Str(cnt);
                if (!cnt)
                    return "";
                string_q retS;
                for (size_t i = 0; i < cnt; i++) {
                    retS += subpages[i].Format();
                    retS += ((i < cnt - 1) ? ",\n" : "\n");
                }
                return retS;
            }
            break;
        case 't':
            if (fieldName % "twoName") {
                return twoName;
            }
            break;
        default:
            break;
    }

    // EXISTING_CODE
    // EXISTING_CODE

    // Finally, give the parent class a chance
    return CBaseNode::getValueByName(fieldName);
}

//---------------------------------------------------------------------------------------------------
bool CPage::setValueByName(const string_q& fieldNameIn, const string_q& fieldValueIn) {
    string_q fieldName = fieldNameIn;
    string_q fieldValue = fieldValueIn;

    // EXISTING_CODE
    // EXISTING_CODE

    switch (tolower(fieldName[0])) {
        case 'c':
            if (fieldName % "color") {
                color = fieldValue;
                return true;
            }
            break;
        case 'f':
            if (fieldName % "files") {
                files = fieldValue;
                return true;
            }
            break;
        case 'h':
            if (fieldName % "has_text") {
                has_text = str_2_Bool(fieldValue);
                return true;
            }
            break;
        case 'l':
            if (fieldName % "longName") {
                longName = fieldValue;
                return true;
            }
            break;
        case 'm':
            if (fieldName % "menuType") {
                menuType = fieldValue;
                return true;
            }
            break;
        case 'n':
            if (fieldName % "no_error") {
                no_error = str_2_Bool(fieldValue);
                return true;
            }
            if (fieldName % "no_data") {
                no_data = str_2_Bool(fieldValue);
                return true;
            }
            if (fieldName % "no_dt") {
                no_dt = str_2_Bool(fieldValue);
                return true;
            }
            if (fieldName % "no_dash") {
                no_dash = str_2_Bool(fieldValue);
                return true;
            }
            break;
        case 'p':
            if (fieldName % "properName") {
                properName = fieldValue;
                return true;
            }
            if (fieldName % "pageNotes") {
                pageNotes = fieldValue;
                return true;
            }
            if (fieldName % "polling") {
                polling = str_2_Bool(fieldValue);
                return true;
            }
            break;
        case 's':
            if (fieldName % "sevenName") {
                sevenName = fieldValue;
                return true;
            }
            if (fieldName % "subpages") {
                CSubpage item;
                string_q str = fieldValue;
                while (item.parseJson3(str)) {
                    subpages.push_back(item);
                    item = CSubpage();  // reset
                }
                return true;
            }
            break;
        case 't':
            if (fieldName % "twoName") {
                twoName = fieldValue;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------
void CPage::finishParse() {
    // EXISTING_CODE
    // EXISTING_CODE
}

//---------------------------------------------------------------------------------------------------
bool CPage::Serialize(CArchive& archive) {
    if (archive.isWriting())
        return SerializeC(archive);

    // Always read the base class (it will handle its own backLevels if any, then
    // read this object's back level (if any) or the current version.
    CBaseNode::Serialize(archive);
    if (readBackLevel(archive))
        return true;

    // EXISTING_CODE
    // EXISTING_CODE
    archive >> longName;
    archive >> properName;
    archive >> twoName;
    archive >> sevenName;
    archive >> pageNotes;
    archive >> subpages;
    archive >> polling;
    archive >> menuType;
    archive >> files;
    archive >> no_error;
    archive >> no_data;
    archive >> no_dt;
    archive >> no_dash;
    archive >> has_text;
    archive >> color;
    finishParse();
    return true;
}

//---------------------------------------------------------------------------------------------------
bool CPage::SerializeC(CArchive& archive) const {
    // Writing always write the latest version of the data
    CBaseNode::SerializeC(archive);

    // EXISTING_CODE
    // EXISTING_CODE
    archive << longName;
    archive << properName;
    archive << twoName;
    archive << sevenName;
    archive << pageNotes;
    archive << subpages;
    archive << polling;
    archive << menuType;
    archive << files;
    archive << no_error;
    archive << no_data;
    archive << no_dt;
    archive << no_dash;
    archive << has_text;
    archive << color;

    return true;
}

//---------------------------------------------------------------------------
CArchive& operator>>(CArchive& archive, CPageArray& array) {
    uint64_t count;
    archive >> count;
    array.resize(count);
    for (size_t i = 0; i < count; i++) {
        ASSERT(i < array.capacity());
        array.at(i).Serialize(archive);
    }
    return archive;
}

//---------------------------------------------------------------------------
CArchive& operator<<(CArchive& archive, const CPageArray& array) {
    uint64_t count = array.size();
    archive << count;
    for (size_t i = 0; i < array.size(); i++)
        array[i].SerializeC(archive);
    return archive;
}

//---------------------------------------------------------------------------
void CPage::registerClass(void) {
    // only do this once
    if (HAS_FIELD(CPage, "schema"))
        return;

    size_t fieldNum = 1000;
    ADD_FIELD(CPage, "schema", T_NUMBER, ++fieldNum);
    ADD_FIELD(CPage, "deleted", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "showing", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "cname", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "longName", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "properName", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "twoName", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "sevenName", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "pageNotes", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "subpages", T_OBJECT | TS_ARRAY, ++fieldNum);
    ADD_FIELD(CPage, "polling", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "menuType", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "files", T_TEXT, ++fieldNum);
    ADD_FIELD(CPage, "no_error", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "no_data", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "no_dt", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "no_dash", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "has_text", T_BOOL, ++fieldNum);
    ADD_FIELD(CPage, "color", T_TEXT, ++fieldNum);

    // Hide our internal fields, user can turn them on if they like
    HIDE_FIELD(CPage, "schema");
    HIDE_FIELD(CPage, "deleted");
    HIDE_FIELD(CPage, "showing");
    HIDE_FIELD(CPage, "cname");

    builtIns.push_back(_biCPage);

    // EXISTING_CODE
    // EXISTING_CODE
}

//---------------------------------------------------------------------------
string_q nextPageChunk_custom(const string_q& fieldIn, const void* dataPtr) {
    const CPage* pag = reinterpret_cast<const CPage*>(dataPtr);
    if (pag) {
        switch (tolower(fieldIn[0])) {
            // EXISTING_CODE
            // EXISTING_CODE
            case 'p':
                // Display only the fields of this node, not it's parent type
                if (fieldIn % "parsed")
                    return nextBasenodeChunk(fieldIn, pag);
                // EXISTING_CODE
                // EXISTING_CODE
                break;

            default:
                break;
        }
    }

    return "";
}

//---------------------------------------------------------------------------
bool CPage::readBackLevel(CArchive& archive) {
    bool done = false;
    // EXISTING_CODE
    // EXISTING_CODE
    return done;
}

//-------------------------------------------------------------------------
ostream& operator<<(ostream& os, const CPage& item) {
    // EXISTING_CODE
    // EXISTING_CODE

    item.Format(os, "", nullptr);
    os << "\n";
    return os;
}

//---------------------------------------------------------------------------
const CBaseNode* CPage::getObjectAt(const string_q& fieldName, size_t index) const {
    if (fieldName % "subpages" && index < subpages.size())
        return &subpages[index];
    return NULL;
}

//---------------------------------------------------------------------------
const char* STR_DISPLAY_PAGE = "";

//---------------------------------------------------------------------------
// EXISTING_CODE
// EXISTING_CODE
}  // namespace qblocks
