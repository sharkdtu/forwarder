#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

int main(int argc, char **argv){
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL, cur = NULL;
    char *name, *value;
    xmlKeepBlanksDefault (0);

    if(argc < 2){
        fprintf(stderr, "usage: xml_reader <filename>\n");
        return -1;
    }

    if((doc = xmlParseFile(argv[1])) == NULL){
        fprintf(stderr, "loading xml file failed\n");
        exit(1);
    }

    if((root = xmlDocGetRootElement(doc)) == NULL){
        fprintf(stderr, "empty file\n");
        xmlFreeDoc(doc);
        exit(1);
    }

    root = root->xmlChildrenNode;
    while(root != NULL){
        cur = root->xmlChildrenNode;
        while(cur != NULL){
            xmlNodePtr gwip = cur->xmlChildrenNode;
	    while(gwip != NULL){
	        name = (char*) gwip->name;
        	value = (char*) xmlNodeGetContent(gwip);
        	printf("DEBUG: name is: %s, value is: %s\n", name, value);
		gwip = gwip->next;
            }
            cur=cur->next;
        }
	root = root->next;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}

