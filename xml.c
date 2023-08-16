#include "libxml/tree.h"
#include "libxml/xmlstring.h"
#include <stdio.h>
#include <libxml/parser.h>

/*gcc `xml2-config --cflags --libs` test.c*/

void parsefix(xmlDocPtr doc, xmlNodePtr cur){
    xmlChar *key;
    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"ident"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("IDENT: %s\t", key);
            xmlFree(key);
        }
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"time_total"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("TIME: %s\t", key);
            xmlFree(key);
        }
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"fuel_plan_onboard"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("FUEL: %s\n", key);
            xmlFree(key);
        }
        cur = cur->next;
    }
}

void parsenavlog(xmlDocPtr doc, xmlNodePtr cur)
{
    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"fix"))) {
            parsefix(doc, cur);
        }
        cur = cur->next;
    }
}

int main()
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile("VHHH.xml");

    if (doc == NULL) {
        fprintf(stderr, "Could not open xml file\n");
        return -1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
        fprintf(stderr, "Empty Document\n");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *)"OFP")) {
        fprintf(stderr, "Document of wrong type, root node !=navlog\n");
        xmlFreeDoc(doc);
        return -1;
    } else {
        printf("Document is correct. Ready to be parsed\n");
    }

    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"navlog")) {
            printf("cur->name: %s\n", cur->name);
            parsenavlog(doc, cur);
        }
        cur = cur->next;
    }
    xmlFreeDoc(doc);

    xmlCleanupParser();
}
