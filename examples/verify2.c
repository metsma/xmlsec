/** 
 * XML Security Library example: Verifying a file using keys manager.
 *
 * Verifies a file using keys manager
 * 
 * Usage: 
 *	verify2 <signed-file> <public-pem-key1> [<public-pem-key2> [...]]
 *
 * Example:
 *	./verify2 sign1-res.xml rsapub.pem
 *	./verify2 sign2-res.xml rsapub.pem
 * 
 * This is free software; see Copyright file in the source
 * distribution for preciese wording.
 * 
 * Copyrigth (C) 2002-2003 Aleksey Sanin <aleksey@aleksey.com>
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifndef XMLSEC_NO_XSLT
#include <libxslt/xslt.h>
#endif /* XMLSEC_NO_XSLT */

#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/crypto.h>

xmlSecKeysMngrPtr load_keys(char** files, int files_size);
int verify_file(xmlSecKeysMngrPtr mngr, const char* xml_file);

int 
main(int argc, char **argv) {
    xmlSecKeysMngrPtr mngr;
    
    assert(argv);

    if(argc < 3) {
	fprintf(stderr, "Error: wrong number of arguments.\n");
	fprintf(stderr, "Usage: %s <xml-file> <key-file1> [<key-file2> [...]]\n", argv[0]);
	return(1);
    }

    /* Init libxml and libxslt libraries */
    xmlInitParser();
    LIBXML_TEST_VERSION
    xmlLoadExtDtdDefaultValue = XML_DETECT_IDS | XML_COMPLETE_ATTRS;
    xmlSubstituteEntitiesDefault(1);
#ifndef XMLSEC_NO_XSLT
    xmlIndentTreeOutput = 1; 
#endif /* XMLSEC_NO_XSLT */
        	
    /* Init xmlsec library */
    if(xmlSecInit() < 0) {
	fprintf(stderr, "Error: xmlsec initialization failed.\n");
	return(-1);
    }

    /* Init crypto library */
    if(xmlSecCryptoAppInit(NULL) < 0) {
	fprintf(stderr, "Error: crypto initialization failed.\n");
	return(-1);
    }

    /* Init xmlsec-crypto library */
    if(xmlSecCryptoInit() < 0) {
	fprintf(stderr, "Error: xmlsec-crypto initialization failed.\n");
	return(-1);
    }

    /* create keys manager and load keys */
    mngr = load_keys(&(argv[2]), argc - 2);
    if(mngr == NULL) {
	return(-1);
    }
    
    /* verify file */
    if(verify_file(mngr, argv[1]) < 0) {
	xmlSecKeysMngrDestroy(mngr);	
	return(-1);
    }    
    
    /* destroy keys manager */
    xmlSecKeysMngrDestroy(mngr);
    
    /* Shutdown xmlsec-crypto library */
    xmlSecCryptoShutdown();
    
    /* Shutdown crypto library */
    xmlSecCryptoAppShutdown();
    
    /* Shutdown xmlsec library */
    xmlSecShutdown();

    /* Shutdown libxslt/libxml */
#ifndef XMLSEC_NO_XSLT
    xsltCleanupGlobals();            
#endif /* XMLSEC_NO_XSLT */
    xmlCleanupParser();
    
    return(0);
}

/**
 * load_keys:
 * @files:		the list of filenames.
 * @files_size:		the number of filenames in #files.
 *
 * Creates simple keys manager and load PEM keys from #files in it.
 * The caller is responsible for destroing returned keys manager using
 * @xmlSecKeysMngrDestroy.
 *
 * Returns the pointer to newly created keys manager or NULL if an error
 * occurs.
 */
xmlSecKeysMngrPtr 
load_keys(char** files, int files_size) {
    xmlSecKeysMngrPtr mngr;
    xmlSecKeyPtr key;
    int i;
    
    assert(files);
    assert(files_size > 0);
    
    /* create and initialize keys manager, we use a simple list based
     * keys manager, implement your own xmlSecKeysStore klass if you need
     * something more sophisticated 
     */
    mngr = xmlSecKeysMngrCreate();
    if(mngr == NULL) {
	fprintf(stderr, "Error: failed to create keys manager.\n");
	return(NULL);
    }
    if(xmlSecCryptoAppSimpleKeysMngrInit(mngr) < 0) {
	fprintf(stderr, "Error: failed to initialize keys manager.\n");
	xmlSecKeysMngrDestroy(mngr);
	return(NULL);
    }    
    
    for(i = 0; i < files_size; ++i) {
	assert(files[i]);

	/* load key */
	key = xmlSecCryptoAppPemKeyLoad(files[i], NULL, NULL, NULL);
	if(key == NULL) {
    	    fprintf(stderr,"Error: failed to load pem key from \"%s\"\n", files[i]);
	    xmlSecKeysMngrDestroy(mngr);
	    return(NULL);
	}

	/* set key name to the file name, this is just an example! */
	if(xmlSecKeySetName(key, BAD_CAST files[i]) < 0) {
    	    fprintf(stderr,"Error: failed to set key name for key from \"%s\"\n", files[i]);
	    xmlSecKeyDestroy(key);
	    xmlSecKeysMngrDestroy(mngr);
	    return(NULL);
	}
	
	/* add key to keys manager, from now on keys manager is responsible 
	 * for destroying key 
	 */
	if(xmlSecCryptoAppSimpleKeysMngrAdoptKey(mngr, key) < 0) {
    	    fprintf(stderr,"Error: failed to add key from \"%s\" to keys manager\n", files[i]);
	    xmlSecKeyDestroy(key);
	    xmlSecKeysMngrDestroy(mngr);
	    return(NULL);
	}
    }

    return(mngr);
}

/** 
 * verify_file:
 * @mngr:		the pointer to keys manager.
 * @xml_file:		the signed XML file name.
 *
 * Verifies XML signature in #xml_file.
 *
 * Returns 0 on success or a negative value if an error occurs.
 */
int 
verify_file(xmlSecKeysMngrPtr mngr, const char* xml_file) {
    xmlDocPtr doc = NULL;
    xmlNodePtr node = NULL;
    xmlSecDSigCtxPtr dsigCtx = NULL;
    int res = -1;
    
    assert(mngr);
    assert(xml_file);

    /* load file */
    doc = xmlParseFile(xml_file);
    if ((doc == NULL) || (xmlDocGetRootElement(doc) == NULL)){
	fprintf(stderr, "Error: unable to parse file \"%s\"\n", xml_file);
	goto done;	
    }
    
    /* find start node */
    node = xmlSecFindNode(xmlDocGetRootElement(doc), xmlSecNodeSignature, xmlSecDSigNs);
    if(node == NULL) {
	fprintf(stderr, "Error: start node not found in \"%s\"\n", xml_file);
	goto done;	
    }

    /* create signature context */
    dsigCtx = xmlSecDSigCtxCreate(mngr);
    if(dsigCtx == NULL) {
        fprintf(stderr,"Error: failed to create signature context\n");
	goto done;
    }

    /* Verify signature */
    if(xmlSecDSigCtxVerify(dsigCtx, node) < 0) {
        fprintf(stderr,"Error: signature verify\n");
	goto done;
    }
        
    /* print verification result to stdout */
    if(dsigCtx->status == xmlSecDSigStatusSucceeded) {
	fprintf(stdout, "Signature is OK\n");
    } else {
	fprintf(stdout, "Signature is INVALID\n");
    }    

    /* success */
    res = 0;

done:    
    /* cleanup */
    if(dsigCtx != NULL) {
	xmlSecDSigCtxDestroy(dsigCtx);
    }
    
    if(doc != NULL) {
	xmlFreeDoc(doc); 
    }
    return(res);
}


