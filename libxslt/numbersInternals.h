/*
 * numbers.h: Implementation of the XSLT number functions
 *
 * See Copyright for the status of this software.
 *
 * Daniel.Veillard@imag.fr
 * Bjorn Reese <breese@users.sourceforge.net>
 */

#ifndef __XML_XSLT_NUMBERSINTERNALS_H__
#define __XML_XSLT_NUMBERSINTERNALS_H__

#include "xsltInternals.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This data structure is just a wrapper to pass data in
 */
typedef struct _xsltNumberData {
    xmlChar *value;
    xmlChar *format;
    int digitsPerGroup;
    xmlChar groupingCharacter;
} xsltNumberData, *xsltNumberDataPtr;

xmlXPathError xsltFormatNumberConversion(xsltDecimalFormatPtr, xmlChar *,
					 double, xmlChar **);
void xsltNumberFormat(xsltTransformContextPtr, xsltNumberDataPtr, xmlNodePtr);

#ifdef __cplusplus
}
#endif
#endif /* __XML_XSLT_NUMBERSINTERNALS_H__ */