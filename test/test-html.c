#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/html.h>

extern bool test_html(void);

static int testCount = 0;
static int testSucceeded = 0;

static char *outputComparison = "<html><head>HEAD</head><body><div>||&quot;'&lt;foo&gt;||</div><div>||FRUNK-&quot;Fudge&quot;||</div>BODY</body></html>";

static char *outputComparisonDiv = "<div class=\"Foo\"></div>";
static char *outputComparisonDiv2 = "<div class=\"Bar Foo\"></div>";
static char *outputComparisonDiv3 = "<div class=\"Bar\"></div>";
static char *outputComparisonDiv4 = "<div class=\"Foo Bar\"></div>";
static char *outputComparisonEmptyDiv = "<div></div>";

static char *div1Contents = "||\"'<foo>||";
static char *div2Contents = "||FRUNK-\"Fudge\"||";

static char *bodyContents = "BODY";
static char *headContents = "HEAD";

bool test_html(void) {
    //Tests go here:

    struct CS_HtmlNode *html = CS_htmlCreateRoot("html", 2048);
    CS_FAIL_ON_NULL( html, "Create root html.", "Failed" );
    if( html ) {
        struct CS_HtmlNode *head = CS_htmlAddContainerAfter( html, "head" );
        CS_FAIL_ON_NULL( head, "Adding head to root.", "Failed." );
        CS_FAIL_ON_FALSE( html->container == head, "Container should be 'head'", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( head, headContents ), "Contents should be set.", "Nope." );
        struct CS_HtmlNode *body = CS_htmlAddContainerAfter( html, "body" );
        CS_FAIL_ON_NULL( body, "Body added.", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( body, bodyContents ), "Setting contents of the body.", "Nope." );
        struct CS_HtmlNode *div = CS_htmlAddContainerAfter( body, "div" );
        CS_FAIL_ON_NULL( div, "Creating div in body.", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( div, div1Contents ), "Adding contents to div.", "Nope." );
        struct CS_HtmlNode *div2 = CS_htmlAddContainerAfter( body, "div" );
        CS_FAIL_ON_NULL( div2, "Creating div2 in body.", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( div2, div2Contents ), "Adding contents to div2.", "Nope." );
        struct CS_StringBuilder *sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparison ) == 0, "Check details straight.", "\n%s\n%s", sb->buffer, outputComparison );
        CS_SB_free(sb);
        CS_htmlFree(html);
    }
    html = CS_htmlCreateRoot("html", 2048); 
    CS_FAIL_ON_NULL( html, "Create root html.", "Failed" );
    if( html ) {
        struct CS_HtmlNode *body = CS_htmlAddContainerAfter( html, "body" );
        CS_htmlSetContents( body, bodyContents );
        struct CS_HtmlNode *head = CS_htmlAddContainerBefore( html, "head" );
        CS_htmlSetContents( head, headContents );
        struct CS_HtmlNode *div2 = CS_htmlAddContainerAfter( body, "div" );
        CS_FAIL_ON_NULL( div2, "Creating div2 in body.", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( div2, div2Contents ), "Adding contents to div2.", "Nope." );
        struct CS_HtmlNode *div = CS_htmlAddContainerBefore( body, "div" );
        CS_FAIL_ON_NULL( div, "Creating div in body.", "Nope." );
        CS_FAIL_ON_NULL( CS_htmlSetContents( div, div1Contents ), "Adding contents to div.", "Nope." );
        struct CS_StringBuilder *sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparison ) == 0, "Check details mixed.", "\n%s\n%s", sb->buffer, outputComparison );
        CS_SB_free(sb);
        CS_htmlFree(html);
    }

    html = CS_htmlCreateRoot("div", 1024);
    CS_FAIL_ON_NULL( html, "Create root div.", "Failed" );
    if( html ) {
        struct CS_StringBuilder *sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparisonEmptyDiv ) == 0, "Check details empty div.", "\n%s\n%s", sb->buffer, outputComparison );
        CS_SB_free(sb);
        CS_htmlAddAttribute( html, "class", "Foo" );
        sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparisonDiv ) == 0, "Check details div.", "\n%s\n%s", sb->buffer, outputComparison );
        CS_SB_free(sb);
        CS_htmlAddAttribute( html, "class", "Bar" );
        sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparisonDiv2 ) == 0, "Check details div2.", "\n%s\n%s", sb->buffer, outputComparisonDiv2 );
        CS_FAIL_ON_NOT_NULL( CS_htmlRemoveAttributeValue( html, "class", "Scunge" ), "Should fail removing an unknown.", "Oops!" );
        CS_FAIL_ON_NOT_NULL( CS_htmlRemoveAttributeIndex( html, "class", 5 ), "Should fail removing an invalid index.", "Oops." );
        CS_FAIL_ON_NULL( CS_htmlRemoveAttributeIndex( html, "class", 1 ), "Remove 1 index", "Nope" );
        sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparisonDiv3 ) == 0, "Check details div3.", "\n%s\n%s", sb->buffer, outputComparisonDiv3 );
        CS_SB_free(sb);
        CS_htmlAddAttribute( html, "class", "Foo" );
        sb = CS_htmlToStringBuilder( html, 1024 );
        CS_FAIL_ON_FALSE( strcmp( sb->buffer, outputComparisonDiv4 ) == 0, "Check details div4.", "\n%s\n%s", sb->buffer, outputComparisonDiv4 );
        CS_SB_free(sb);


    }

    return testCount !=
           testSucceeded;
}


