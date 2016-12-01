#include <iostream>
#include <vector>
#include <xercesc/sax2/Attributes.hpp>
#include "memorypacket.hpp"
#include "mux.hpp"
#include "ControlThread.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"


using namespace std;
using namespace xercesc;

enum lackeyml_type {
    instruction,
    load,
    store,
    modify
};


static map<string, lackeyml_type> lackeyml_map;


SAX2Handler::SAX2Handler()
{ 
	memoryHandler = nullptr;
    lackeyml_map["instruction"] = instruction;
    lackeyml_map["load"] = load;
    lackeyml_map["store"] = store;
    lackeyml_map["modify"] = modify;
}

void SAX2Handler::setMemoryHandler(XMLFunctor *handler)
{
    memoryHandler = handler;
}

void SAX2Handler::startElement(const XMLCh* const uri,
	const XMLCh* const localname, const XMLCh* const qname,
	const Attributes& attrs)
{

	//test code
    char *message = XMLString::transcode(localname);
    cout << "Element: " << message << endl;
    XMLString::release(&message);

    XMLCh *addressStr = XMLString::transcode("address");
    XMLCh *sizeStr = XMLString::transcode("size");
    char *address = XMLString::transcode(attrs.getValue(addressStr));
    char *size = XMLString::transcode(attrs.getValue(sizeStr));
    char *memAccess = XMLString::transcode(localname);
    switch (lackeyml_map[memAccess]) {
        case instruction:
            cout << address << ":" << size << endl;
            XMLString::release(&address);
            XMLString::release(&size);
        break;
        default:
        break;
    }

}

void SAX2Handler::fatalError(const SAXParseException& exception)
{
    char *message = XMLString::transcode(exception.getMessage());
	cout << "Fatal error: " << message << " at line ";
	cout << exception.getLineNumber() << endl;
	XMLString::release(&message);
}

