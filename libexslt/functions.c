#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/hash.h>

#include <libxslt/xsltutils.h>
#include <libxslt/variables.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

typedef struct _exsltFuncFunctionData exsltFuncFunctionData;
struct _exsltFuncFunctionData {
    int nargs;
    xmlNodePtr content;
};

typedef struct _exsltFuncData exsltFuncData;
struct _exsltFuncData {
    xmlHashTablePtr funcs;
    xmlXPathObjectPtr result;
    int error;
};

static void exsltFuncFunctionElem (xsltTransformContextPtr ctxt,
				   xmlNodePtr node, xmlNodePtr inst,
				   xsltStylePreCompPtr comp);
static void exsltFuncResultElem (xsltTransformContextPtr ctxt,
				 xmlNodePtr node, xmlNodePtr inst,
				 xsltStylePreCompPtr comp);

/**
 * exsltFuncInit:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI for the extension
 *
 * Initializes the EXSLT - Functions module.
 *
 * Returns the data for this transformation
 */
static exsltFuncData *
exsltFuncInit (xsltTransformContextPtr ctxt, const xmlChar *URI) {
    exsltFuncData *ret;

    ret = (exsltFuncData *) xmlMalloc (sizeof(exsltFuncData));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncInit: not enough memory\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(exsltFuncData));

    ret->funcs = xmlHashCreate(1);
    ret->result = NULL;
    ret->error = 0;

    xsltRegisterExtElement (ctxt, (const xmlChar *) "function",
			    URI, exsltFuncFunctionElem);
    xsltRegisterExtElement (ctxt, (const xmlChar *) "result",
			    URI, exsltFuncResultElem);


    return(ret);
}

/**
 * exsltFuncShutdown:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI fir the extension
 * @data: the module data to free up
 *
 * Shutdown the EXSLT - Functions module
 */
static void
exsltFuncShutdown (xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		   const xmlChar *URI ATTRIBUTE_UNUSED,
		   exsltFuncData *data) {
    if (data->funcs != NULL)
	xmlHashFree(data->funcs, (xmlHashDeallocator) xmlFree);
    if (data->result != NULL)
	xmlXPathFreeObject(data->result);
    xmlFree(data);
}

/**
 * exsltFuncRegister:
 *
 * Registers the EXSLT - Functions module
 */
void
exsltFuncRegister (void) {
    xsltRegisterExtModule (EXSLT_FUNCTIONS_NAMESPACE,
			   (xsltExtInitFunction) exsltFuncInit,
			   (xsltExtShutdownFunction) exsltFuncShutdown);
}

/**
 * exsltFuncNewFunctionData:
 *
 * Allocates an #exslFuncFunctionData object
 *
 * Returns the new structure
 */
static exsltFuncFunctionData *
exsltFuncNewFunctionData (void) {
    exsltFuncFunctionData *ret;

    ret = (exsltFuncFunctionData *) xmlMalloc (sizeof(exsltFuncFunctionData));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncNewFunctionData: not enough memory\n");
	return (NULL);
    }

    ret->nargs = 0;
    ret->content = NULL;

    return(ret);
}

/**
 * exsltFuncFunctionFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Evaluates the func:function element defining the called function.
 */
static void
exsltFuncFunctionFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlXPathObjectPtr obj, oldResult, ret;
    exsltFuncData *data;
    exsltFuncFunctionData *func;
    xmlNodePtr paramNode, oldInsert, fake;
    xsltStackElemPtr params = NULL, param;
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
    int i;

    /*
     * retrieve func:function template
     */
    data = (exsltFuncData *) xsltGetExtData (tctxt,
					     EXSLT_FUNCTIONS_NAMESPACE);
    oldResult = data->result;
    data->result = NULL;

    func = (exsltFuncFunctionData*) xmlHashLookup2 (data->funcs,
						    ctxt->context->functionURI,
						    ctxt->context->function);

    /*
     * params handling
     */
    if (nargs > func->nargs) {
	xsltGenericError(xsltGenericErrorContext,
			 "{%s}%s: called with too many arguments\n",
			 ctxt->context->functionURI, ctxt->context->function);
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    if (func->content != NULL)
	paramNode = func->content->prev;
    else
	paramNode = NULL;
    if ((paramNode == NULL) && (func->nargs != 0)) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncFunctionFunction: nargs != 0 and "
			 "param == NULL\n");
	return;
    }
    /* defaulted params */
    for (i = func->nargs; (i > nargs) && (paramNode != NULL); i--) {
	param = xsltParseStylesheetCallerParam (tctxt, paramNode);
	param->next = params;
	params = param;
	paramNode = paramNode->prev;
    }
    /* set params */
    while ((i-- > 0) && (paramNode != NULL)) {
	obj = valuePop(ctxt);
	/* FIXME: this is a bit hackish */
	param = xsltParseStylesheetCallerParam (tctxt, paramNode);
	param->computed = 1;
	if (param->value != NULL)
	    xmlXPathFreeObject(param->value);
	param->value = obj;
	param->next = params;
	params = param;
	paramNode = paramNode->prev;
    }

    /*
     * actual processing
     */
    fake = xmlNewDocNode(tctxt->output, NULL,
			 (const xmlChar *)"fake", NULL);
    oldInsert = tctxt->insert;
    tctxt->insert = fake;
    xsltApplyOneTemplate (tctxt, xmlXPathGetContextNode(ctxt),
			  func->content, NULL, params);
    tctxt->insert = oldInsert;

    if (data->error != 0)
	return;

    if (data->result != NULL)
	ret = data->result;
    else
	ret = xmlXPathNewCString("");

    data->result = oldResult;

    /*
     * It is an error if the instantiation of the template results in
     * the generation of result nodes.
     */
    if (fake->children != NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "{%s}%s: cannot write to result tree while "
			 "executing a function\n",
			 ctxt->context->functionURI, ctxt->context->function);
	return;
    }
    valuePush(ctxt, ret);
}

static void
exsltFuncFunctionElem (xsltTransformContextPtr ctxt, xmlNodePtr node,
		       xmlNodePtr inst, xsltStylePreCompPtr comp) {
    xmlChar *name, *prefix;
    xmlNsPtr ns;
    exsltFuncData *data;
    exsltFuncFunctionData *func;
    xsltStylePreCompPtr param_comp;

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL) || (comp == NULL))
	return;

    {
	xmlChar *qname;

	qname = xmlGetProp(inst, (const xmlChar *) "name");
	name = xmlSplitQName2 (qname, &prefix);
	xmlFree(qname);
    }
    if ((name == NULL) || (prefix == NULL)) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:function: not a QName\n");
	if (name != NULL)
	    xmlFree(name);
	return;
    }
    /* namespace lookup */
    ns = xmlSearchNs (inst->doc, inst, prefix);
    if (ns == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:function: undeclared prefix %s\n",
			 prefix);
	xmlFree(name);
	xmlFree(prefix);
	return;
    }
    xmlFree(prefix);

    /*
     * Create function data
     */
    func = exsltFuncNewFunctionData();
    func->content = inst->children;
    param_comp = (xsltStylePreCompPtr) func->content->_private;
    while ((param_comp != NULL) && (param_comp->type == XSLT_FUNC_PARAM)) {
	func->content = func->content->next;
	func->nargs++;
	param_comp = (xsltStylePreCompPtr) func->content->_private;
    }

    /*
     * Register the function data such that it can be retrieved
     * by exslFuncFunctionFunction
     */
    data = (exsltFuncData *) xsltGetExtData (ctxt, EXSLT_FUNCTIONS_NAMESPACE);
    xmlHashAddEntry2 (data->funcs, ns->href, name, func);

    /*
     * Register the function such that it is available for use in XPath
     * expressions.
     */
    xsltRegisterExtFunction (ctxt, name, ns->href,
			     exsltFuncFunctionFunction);

    xmlFree(name);
}

static void
exsltFuncResultElem (xsltTransformContextPtr ctxt, xmlNodePtr node,
		     xmlNodePtr inst, xsltStylePreCompPtr comp) {
    xmlNodePtr test;
    xmlChar *select;
    exsltFuncData *data;
    xmlXPathObjectPtr ret;

    /*
     * "Validity" checking
     */
    /* it is an error to have any following sibling elements aside
     * from the xsl:fallback element.
     */
    for (test = inst->next; test != NULL; test = test->next) {
	if (test->type != XML_ELEMENT_NODE)
	    continue;
	if (IS_XSLT_ELEM(test) && IS_XSLT_NAME(test, "fallback"))
	    continue;
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncResultElem: only xsl:fallback is "
			 "allowed to follow func:result\n");
	return;
    }
    /* it is an error for a func:result element to not be a descendant
     * of func:function.
     * it is an error if a func:result occurs within a func:result
     * element.
     * it is an error if instanciating the content of a variable
     * binding element (i.e. xsl:variable, xsl:param) results in the 
     * instanciation of a func:result element.
     */
    for (test = inst->parent; test != NULL; test = test->parent) {
	if ((test->ns != NULL) &&
	    (xmlStrEqual(test->ns->href, EXSLT_FUNCTIONS_NAMESPACE))) {
	    if (xmlStrEqual(test->name, (const xmlChar *) "function")) {
		break;
	    }
	    if (xmlStrEqual(test->name, (const xmlChar *) "result")) {
		xsltGenericError(xsltGenericErrorContext,
				 "func:result element not allowed within"
				 " another func:result element\n");
		return;
	    }
	}
	if (IS_XSLT_ELEM(test) &&
	    (IS_XSLT_NAME(test, "variable") ||
	     IS_XSLT_NAME(test, "param"))) {
	    xsltGenericError(xsltGenericErrorContext,
			     "func:result element not allowed within"
			     " a variable binding element\n");
	    return;
	}
    }
    /* It is an error if instantiating the content of the
     * func:function element results in the instantiation of more than
     * one func:result elements.
     */
    data = (exsltFuncData *) xsltGetExtData (ctxt, EXSLT_FUNCTIONS_NAMESPACE);
    if (data == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncReturnElem: data == NULL\n");
	return;
    }
    if (data->result != NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:result already instanciated\n");
	data->error = 1;
	return;
    }

    /*
     * Processing
     */
    select = xmlGetProp(inst, (const xmlChar *) "select");
    if (select != NULL) {
	/* If the func:result element has a select attribute, then the
	 * value of the attribute must be an expression and the
	 * returned value is the object that results from evaluating
	 * the expression. In this case, the content must be empty.
	 */
	if (inst->children != NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "func:result content must be empty if it"
			     " has a select attribute\n");
	    data->error = 1;
	    return;
	}
	ret = xmlXPathEvalExpression(select, ctxt->xpathCtxt);
	if (ret == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "exsltFuncResultElem: ret == NULL\n");
	    return;
	}
    } else if (test->children != NULL) {
	/* If the func:result element does not have a select attribute
	 * and has non-empty content (i.e. the func:result element has
	 * one or more child nodes), then the content of the
	 * func:result element specifies the value.
	 */
	xmlNodePtr container, oldInsert;

	container = xmlNewDocNode (ctxt->output, NULL,
				   (const xmlChar *) "fake", NULL);
	oldInsert = ctxt->insert;
	ctxt->insert = container;
	xsltApplyOneTemplate (ctxt, ctxt->xpathCtxt->node,
			      inst->children, NULL, NULL);
	ctxt->insert = oldInsert;

	ret = xmlXPathNewValueTree(container);
	if (ret == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "exsltFuncResultElem: ret == NULL\n");
	    data->error = 1;
	}
    } else {
	/* If the func:result element has empty content and does not
	 * have a select attribute, then the returned value is an
	 * empty string.
	 */
	ret = xmlXPathNewCString("");
    }
    data->result = ret;
}