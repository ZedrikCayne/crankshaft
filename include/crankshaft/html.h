#ifndef __crankshafthtmldoth__
#define __crankshafthtmldoth__
#include <stdbool.h>

#include <crankshaft/stringbuilder.h>


/********************************************************************
 *
 * Basic HTML generation. All based on a linear allocator so values
 * are always copied in. No individual item's size may exceed the
 * initial alloc of the root node.
 *
 * See main.cpp for an example or two.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

struct CS_HtmlValue {
    char *value;
    struct CS_HtmlValue *next;
};

struct CS_HtmlAttribute {
    char *name;
    char *separator;
    struct CS_HtmlValue *values;
    struct CS_HtmlAttribute *next;
};

struct CS_HtmlNode {
	char *name;
    struct CS_HtmlAttribute *attributes;
    struct CS_HtmlNode *container;
    struct CS_HtmlNode *next;
    struct CS_HtmlNode *last;
    struct CS_HtmlNode *up;
    bool raw;
    char *contents;
    void *linearAllocator;
};

struct CS_HtmlNode *CS_htmlCreateRoot(const char *name, int initialAlloc);
struct CS_HtmlAttribute *CS_htmlAddAttribute(struct CS_HtmlNode *node, const char *attributeName, const char *attributeValue);
struct CS_HtmlAttribute *CS_htmlGetAttributes(struct CS_HtmlNode *node, const char *attributeName);
struct CS_HtmlNode *CS_htmlRemoveAttributeIndex(struct CS_HtmlNode *node, const char *attributeName, int index ); 
struct CS_HtmlNode *CS_htmlRemoveAttributeValue(struct CS_HtmlNode *node, const char *attributeName, const char *attributeValue );
struct CS_HtmlNode *CS_htmlRemoveAttribute(struct CS_HtmlNode *node, const char *attributeName);
struct CS_HtmlNode *CS_htmlAddNext(struct CS_HtmlNode *node, const char *name);
struct CS_HtmlNode *CS_htmlAddBefore(struct CS_HtmlNode *node, const char *name);
struct CS_HtmlNode *CS_htmlAddContainerBefore(struct CS_HtmlNode *node, const char *name);
struct CS_HtmlNode *CS_htmlAddContainerAfter(struct CS_HtmlNode *node, const char *name);
struct CS_HtmlNode *CS_htmlSetContents(struct CS_HtmlNode *node, const char *contents, bool raw);
void CS_htmlFree(struct CS_HtmlNode *node);
struct CS_StringBuilder *CS_htmlToStringBuilder(struct CS_HtmlNode *node, int initialSize, bool pretty);

#ifdef __cplusplus
}
#endif
#endif
