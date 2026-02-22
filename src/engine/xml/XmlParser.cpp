// XmlParser.cpp — Real XML parser for OpenCiv4 using pugixml.
//
// Implements all CvDLLXmlIFaceBase virtual methods, replacing the no-op
// StubXmlIFace so that CvXMLLoadUtility can actually load BTS XML data.
//
// FXml is defined here as a struct wrapping a pugixml document + cursor.
// FXmlSchemaCache is a dummy (BTS used Xerces schema validation; we skip it).

#include "CvGameCoreDLL.h"
#include "CvDLLXMLIFaceBase.h"

// pugixml lives next to this file
#include "pugixml.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>


// ============================================================================
//  FXml — the opaque type that gamecore passes around
// ============================================================================
struct FXml
{
    pugi::xml_document doc;       // the loaded XML document
    pugi::xml_node     cursor;    // "last located node" — current position in the tree
};

// ============================================================================
//  FXmlSchemaCache — dummy (no schema validation in OpenCiv4)
// ============================================================================
struct FXmlSchemaCache
{
    int dummy;
};

// ============================================================================
//  Helper: normalize a game-relative XML path
//
//  The gamecore calls LoadCivXml which prepends "Assets//" when
//  fileManagerEnabled() returns false. So we receive paths like:
//      "Assets//xml/GlobalDefines.xml"
//  We normalize double slashes and return the relative portion (e.g.
//  "Assets/xml/GlobalDefines.xml") for use with layered path search.
// ============================================================================
static std::string NormalizeXmlPath(const char* pszXmlFile)
{
    if (!pszXmlFile) return "";

    std::string path(pszXmlFile);

    // Normalize double slashes to single
    std::string::size_type pos;
    while ((pos = path.find("//")) != std::string::npos)
        path.replace(pos, 2, "/");

    return path;
}

// ============================================================================
//  Helper: compute the parent directory of BTS_INSTALL_DIR
//
//  BTS_INSTALL_DIR = ".../Sid Meier's Civilization IV Beyond the Sword/Beyond the Sword"
//  Parent          = ".../Sid Meier's Civilization IV Beyond the Sword/"
// ============================================================================
static std::string GetParentDir()
{
    std::string bts = BTS_INSTALL_DIR;
    // Strip trailing slashes
    while (!bts.empty() && (bts.back() == '/' || bts.back() == '\\'))
        bts.pop_back();
    // Find last separator
    auto pos = bts.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return bts.substr(0, pos + 1); // include trailing slash
}

// ============================================================================
//  Layered file search — mimics the real Civ4 file system:
//    1. Beyond the Sword/Assets/...  (BTS — checked first)
//    2. Warlords/Assets/...          (expansion — middle layer)
//    3. Assets/...                   (base Civ4 — final fallback)
//
//  Returns the first path where the file actually exists, or empty string.
// ============================================================================
static std::string ResolveXmlPath(const char* pszXmlFile)
{
    std::string relPath = NormalizeXmlPath(pszXmlFile);
    if (relPath.empty()) return "";

    // If path is already absolute, use it directly
    if (relPath.size() >= 2 && relPath[1] == ':')
        return relPath;
    if (!relPath.empty() && (relPath[0] == '/' || relPath[0] == '\\'))
        return relPath;

    std::string bts = BTS_INSTALL_DIR;
    if (!bts.empty() && bts.back() != '/' && bts.back() != '\\')
        bts += '/';

    std::string parent = GetParentDir();

    // Search order: BTS → Warlords → Base game
    std::string candidates[3] = {
        bts + relPath,                           // Beyond the Sword
        parent + "Warlords/" + relPath,          // Warlords
        parent + relPath,                        // Base Civ4
    };

    for (const auto& candidate : candidates)
    {
        FILE* f = fopen(candidate.c_str(), "rb");
        if (f)
        {
            fclose(f);
            return candidate;
        }
    }

    // Nothing found — return BTS path (will produce a clear error message)
    return candidates[0];
}

// ============================================================================
//  Helper: navigate a slash-separated path from the document root
//
//  LocateNode("Civ4Defines/Define") means:
//    1. Go to root's child named "Civ4Defines"
//    2. Go to its child named "Define"
//    3. Set cursor to that first "Define" element
// ============================================================================
static pugi::xml_node NavigatePath(pugi::xml_document& doc, const char* pszPath)
{
    if (!pszPath || !*pszPath) return pugi::xml_node();

    std::string path(pszPath);
    pugi::xml_node current = doc.document_element();

    // Split on '/'
    std::string::size_type start = 0;
    std::string::size_type end;

    // The first segment should match the document element name
    end = path.find('/', start);
    std::string firstSeg = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);

    if (!current || strcmp(current.name(), firstSeg.c_str()) != 0)
    {
        // Try as a child of the document (no document element match)
        current = doc.child(firstSeg.c_str());
        if (!current) return pugi::xml_node();
    }

    if (end == std::string::npos)
        return current; // path was just "RootElement"

    start = end + 1;

    // Navigate remaining segments
    while (start < path.size())
    {
        end = path.find('/', start);
        std::string segment = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);

        current = current.child(segment.c_str());
        if (!current) return pugi::xml_node();

        if (end == std::string::npos) break;
        start = end + 1;
    }

    return current;
}

// ============================================================================
//  Helper: get the text content of the cursor node
//
//  In BTS XML files, element values are stored as text content:
//    <DefineTag>42</DefineTag>
//  pugixml returns this via child_value() or text().get()
// ============================================================================
static const char* GetNodeText(const pugi::xml_node& node)
{
    // For elements like <Tag>value</Tag>, text() returns "value"
    const char* txt = node.text().get();
    if (txt && *txt) return txt;

    // For text/PCDATA nodes themselves
    txt = node.value();
    if (txt && *txt) return txt;

    // For child text node
    pugi::xml_node child = node.first_child();
    if (child && child.type() == pugi::node_pcdata)
        return child.value();

    return "";
}

// ============================================================================
//  PugiXmlIFace — the real implementation
// ============================================================================
namespace OpenCiv4 {

class PugiXmlIFace : public CvDLLXmlIFaceBase
{
public:
    // --- Lifecycle ---

    FXml* CreateFXml(FXmlSchemaCache* /*pSchemaCache*/) override
    {
        return new FXml();
    }

    void DestroyFXml(FXml*& xml) override
    {
        delete xml;
        xml = nullptr;
    }

    FXmlSchemaCache* CreateFXmlSchemaCache() override
    {
        return new FXmlSchemaCache();
    }

    void DestroyFXmlSchemaCache(FXmlSchemaCache*& cache) override
    {
        delete cache;
        cache = nullptr;
    }

    // --- Loading ---

    bool LoadXml(FXml* xml, const TCHAR* pszXmlFile) override
    {
        if (!xml || !pszXmlFile) return false;

        std::string fullPath = ResolveXmlPath(pszXmlFile);

        pugi::xml_parse_result result = xml->doc.load_file(fullPath.c_str());
        if (!result)
        {
            fprintf(stderr, "[XmlParser] FAILED to load '%s': %s (offset %td)\n",
                    fullPath.c_str(), result.description(), result.offset);
            return false;
        }

        // Set cursor to the document element
        xml->cursor = xml->doc.document_element();
        return true;
    }

    bool Validate(FXml* /*xml*/, TCHAR* /*pszError*/) override
    {
        return true; // No schema validation
    }

    // --- Navigation ---

    bool LocateNode(FXml* xml, const TCHAR* pszXmlNode) override
    {
        if (!xml || !pszXmlNode) return false;

        pugi::xml_node found = NavigatePath(xml->doc, pszXmlNode);
        if (!found) return false;

        xml->cursor = found;
        return true;
    }

    bool LocateFirstSiblingNodeByTagName(FXml* xml, TCHAR* pszTagName) override
    {
        if (!xml || !pszTagName) return false;

        // Go to parent, then find first child with tag name
        pugi::xml_node parent = xml->cursor.parent();
        if (!parent) return false;

        pugi::xml_node found = parent.child(pszTagName);
        if (!found) return false;

        xml->cursor = found;
        return true;
    }

    bool LocateNextSiblingNodeByTagName(FXml* xml, TCHAR* pszTagName) override
    {
        if (!xml || !pszTagName) return false;

        pugi::xml_node next = xml->cursor.next_sibling(pszTagName);
        if (!next) return false;

        xml->cursor = next;
        return true;
    }

    bool NextSibling(FXml* xml) override
    {
        if (!xml) return false;

        // Skip non-element nodes (text, comments, etc.)
        pugi::xml_node next = xml->cursor.next_sibling();
        while (next && next.type() != pugi::node_element)
            next = next.next_sibling();

        if (!next) return false;

        xml->cursor = next;
        return true;
    }

    bool PrevSibling(FXml* xml) override
    {
        if (!xml) return false;

        pugi::xml_node prev = xml->cursor.previous_sibling();
        while (prev && prev.type() != pugi::node_element)
            prev = prev.previous_sibling();

        if (!prev) return false;

        xml->cursor = prev;
        return true;
    }

    bool SetToChild(FXml* xml) override
    {
        if (!xml) return false;

        pugi::xml_node child = xml->cursor.first_child();
        // Skip non-element children (text, pcdata, comments)
        while (child && child.type() != pugi::node_element)
            child = child.next_sibling();

        if (!child) return false;

        xml->cursor = child;
        return true;
    }

    bool SetToChildByTagName(FXml* xml, const TCHAR* szTagName) override
    {
        if (!xml || !szTagName) return false;

        pugi::xml_node child = xml->cursor.child(szTagName);
        if (!child) return false;

        xml->cursor = child;
        return true;
    }

    bool SetToParent(FXml* xml) override
    {
        if (!xml) return false;

        pugi::xml_node parent = xml->cursor.parent();
        if (!parent) return false;

        xml->cursor = parent;
        return true;
    }

    // --- Value access ---

    int GetLastNodeTextSize(FXml* xml) override
    {
        if (!xml) return 0;
        const char* txt = GetNodeText(xml->cursor);
        return (int)strlen(txt);
    }

    bool GetLastNodeText(FXml* xml, TCHAR* pszText) override
    {
        if (!xml || !pszText) return false;
        const char* txt = GetNodeText(xml->cursor);
        strcpy(pszText, txt);
        return true;
    }

    bool GetLastNodeValue(FXml* xml, std::string& pszText) override
    {
        if (!xml) return false;
        pszText = GetNodeText(xml->cursor);
        return true;
    }

    bool GetLastNodeValue(FXml* xml, std::wstring& pszText) override
    {
        if (!xml) return false;
        const char* txt = GetNodeText(xml->cursor);
        // Convert narrow to wide
        pszText.clear();
        while (*txt)
        {
            pszText += (wchar_t)(unsigned char)*txt;
            ++txt;
        }
        return true;
    }

    bool GetLastNodeValue(FXml* xml, char* pszText) override
    {
        if (!xml || !pszText) return false;
        strcpy(pszText, GetNodeText(xml->cursor));
        return true;
    }

    bool GetLastNodeValue(FXml* xml, wchar* pszText) override
    {
        if (!xml || !pszText) return false;
        const char* txt = GetNodeText(xml->cursor);
        int i = 0;
        while (txt[i])
        {
            pszText[i] = (wchar_t)(unsigned char)txt[i];
            i++;
        }
        pszText[i] = 0;
        return true;
    }

    bool GetLastNodeValue(FXml* xml, bool* pbVal) override
    {
        if (!xml || !pbVal) return false;
        const char* txt = GetNodeText(xml->cursor);
        // BTS XML uses "1", "true", "True" for true
        *pbVal = (strcmp(txt, "1") == 0 || _stricmp(txt, "true") == 0);
        return true;
    }

    bool GetLastNodeValue(FXml* xml, int* piVal) override
    {
        if (!xml || !piVal) return false;
        const char* txt = GetNodeText(xml->cursor);
        *piVal = atoi(txt);
        return true;
    }

    bool GetLastNodeValue(FXml* xml, float* pfVal) override
    {
        if (!xml || !pfVal) return false;
        const char* txt = GetNodeText(xml->cursor);
        *pfVal = (float)atof(txt);
        return true;
    }

    bool GetLastNodeValue(FXml* xml, unsigned int* puiVal) override
    {
        if (!xml || !puiVal) return false;
        const char* txt = GetNodeText(xml->cursor);
        *puiVal = (unsigned int)strtoul(txt, nullptr, 10);
        return true;
    }

    // --- Node type / tag info ---

    bool GetLastLocatedNodeType(FXml* xml, TCHAR* pszType) override
    {
        if (!xml || !pszType) return false;

        // BTS GlobalDefines.xml uses:
        //   <Define>
        //     <DefineName>FOO</DefineName>
        //     <fVal>1.5</fVal>      ← type is "float"
        //     <iVal>42</iVal>       ← type is "int"
        //     <DefineTextVal>BAR</DefineTextVal> ← type is "" (string)
        //   </Define>
        //
        // GetLastLocatedNodeType checks what type of value node we're on.
        // The tag name itself encodes the type:
        //   "fVal" → "float", "iVal" → "int", "bVal" → "boolean"

        const char* tagName = xml->cursor.name();
        if (!tagName || !*tagName)
        {
            strcpy(pszType, "");
            return false;
        }

        if (strcmp(tagName, "fVal") == 0 || strcmp(tagName, "FloatVal") == 0 || strcmp(tagName, "fDefineFloatVal") == 0)
            strcpy(pszType, "float");
        else if (strcmp(tagName, "iVal") == 0 || strcmp(tagName, "IntVal") == 0 || strcmp(tagName, "iDefineIntVal") == 0)
            strcpy(pszType, "int");
        else if (strcmp(tagName, "bVal") == 0 || strcmp(tagName, "BoolVal") == 0 || strcmp(tagName, "bDefineBoolVal") == 0)
            strcpy(pszType, "boolean");
        else if (strcmp(tagName, "DefineTextVal") == 0)
            strcpy(pszType, "");
        else
        {
            // Check the "type" attribute if present (some schemas use this)
            const char* typeAttr = xml->cursor.attribute("type").as_string("");
            strcpy(pszType, typeAttr);
        }

        return true;
    }

    bool GetLastInsertedNodeType(FXml* /*xml*/, TCHAR* pszType) override
    {
        if (pszType) pszType[0] = 0;
        return false;
    }

    bool IsLastLocatedNodeCommentNode(FXml* xml) override
    {
        if (!xml) return false;
        return xml->cursor.type() == pugi::node_comment;
    }

    bool GetLastLocatedNodeTagName(FXml* xml, TCHAR* pszTagName) override
    {
        if (!xml || !pszTagName) return false;
        const char* name = xml->cursor.name();
        if (!name || !*name) return false;
        strcpy(pszTagName, name);
        return true;
    }

    // --- Counting ---

    int NumOfElementsByTagName(FXml* xml, TCHAR* pszTagName) override
    {
        if (!xml || !pszTagName) return 0;

        // Count all elements with this tag name at the same level as cursor
        // (including cursor itself if it matches)
        pugi::xml_node parent = xml->cursor.parent();
        if (!parent) return 0;

        int count = 0;
        for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling())
        {
            if (child.type() == pugi::node_element && strcmp(child.name(), pszTagName) == 0)
                count++;
        }
        return count;
    }

    int NumOfChildrenByTagName(FXml* xml, const TCHAR* pszTagName) override
    {
        if (!xml || !pszTagName) return 0;

        int count = 0;
        for (pugi::xml_node child = xml->cursor.first_child(); child; child = child.next_sibling())
        {
            if (child.type() == pugi::node_element && strcmp(child.name(), pszTagName) == 0)
                count++;
        }
        return count;
    }

    int GetNumSiblings(FXml* xml) override
    {
        if (!xml) return 0;

        // Count siblings AFTER the current node (not including current)
        int count = 0;
        pugi::xml_node sib = xml->cursor.next_sibling();
        while (sib)
        {
            if (sib.type() == pugi::node_element)
                count++;
            sib = sib.next_sibling();
        }
        return count;
    }

    int GetNumChildren(FXml* xml) override
    {
        if (!xml) return 0;

        int count = 0;
        for (pugi::xml_node child = xml->cursor.first_child(); child; child = child.next_sibling())
        {
            if (child.type() == pugi::node_element)
                count++;
        }
        return count;
    }

    // --- Write/insert methods (not needed for loading — all no-ops) ---

    bool AddChildNode(FXml* /*xml*/, TCHAR* /*pszNewNode*/) override { return false; }
    bool AddSiblingNodeBefore(FXml* /*xml*/, TCHAR* /*pszNewNode*/) override { return false; }
    bool AddSiblingNodeAfter(FXml* /*xml*/, TCHAR* /*pszNewNode*/) override { return false; }
    bool WriteXml(FXml* /*xml*/, TCHAR* /*pszXmlFile*/) override { return false; }
    bool SetInsertedNodeAttribute(FXml* /*xml*/, TCHAR* /*pszAttributeName*/, TCHAR* /*pszAttributeValue*/) override { return false; }
    int  GetInsertedNodeTextSize(FXml* /*xml*/) override { return 0; }
    bool GetInsertedNodeText(FXml* /*xml*/, TCHAR* /*pszText*/) override { return false; }
    bool SetInsertedNodeText(FXml* /*xml*/, TCHAR* /*pszText*/) override { return false; }

    // --- Caching / mapping (no-ops) ---

    bool IsAllowXMLCaching() override { return false; }
    void MapChildren(FXml*) override {}
};

} // namespace OpenCiv4

// ============================================================================
//  Factory function — called from StubInterfaces.h to create the real parser
// ============================================================================
CvDLLXmlIFaceBase* OpenCiv4_CreateXmlParser()
{
    return new OpenCiv4::PugiXmlIFace();
}
