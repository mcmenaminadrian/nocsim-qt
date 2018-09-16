#include <iostream>
#include <vector>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <mutex>
#include "memorypacket.hpp"
#include "mux.hpp"
#include "ControlThread.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"


using namespace xercesc;
using namespace std;

#define instruction 0
#define load 1
#define store 2
#define modify 3
#define nothing 4

static const int parseType(string memType)
{
    if (memType == "instruction") {
           return instruction;
    }
    else if (memType == "load") {
        return load;
    }
    else if (memType == "store") {
        return store;
    }
    else if (memType == "modify") {
        return modify;
    }
    return nothing;
}


SAX2Handler::SAX2Handler()
{ 
	memoryHandler = nullptr;
}

void SAX2Handler::setMemoryHandler(XMLFunctor *handler)
{
    memoryHandler = handler;
}

void SAX2Handler::startElement(const XMLCh* const uri,
	const XMLCh* const localname, const XMLCh* const qname,
	const Attributes& attrs)
{
    XMLCh *addressStr = XMLString::transcode("address");
    XMLCh *sizeStr = XMLString::transcode("size");

    char *memAccess = XMLString::transcode(localname);
    auto typeXML = parseType(memAccess);
    char *address = XMLString::transcode(attrs.getValue(addressStr));
    char *size = XMLString::transcode(attrs.getValue(sizeStr));
    if (address) {
        string addrStr(address);
        uint64_t uiAddress = stol(addrStr, nullptr, 16);
        switch (typeXML) {
            case instruction:
                memoryHandler->proc->setProgramCounter(uiAddress);
                memoryHandler->proc->pcAdvance(atoi(size));
                XMLString::release(&address);
                XMLString::release(&size);
                XMLString::release(&addressStr);
                XMLString::release(&sizeStr);
		XMLString::release(&memAccess);
                break;
            case store:

                switch (stoi(size)) {
                case 8:
                    memoryHandler->proc->writeAddress64(uiAddress);
                    break;
                case 4:
                    memoryHandler->proc->writeAddress32(uiAddress);
                    break;
                case 2:
                    memoryHandler->proc->writeAddress16(uiAddress);
		    break;
                case 1:
                    memoryHandler->proc->writeAddress8(uiAddress);
                    break;
                default:
                    cerr << "BAD SIZE: " << size << endl;
                    exit(1);
                }
                XMLString::release(&address);
                XMLString::release(&size);
                XMLString::release(&addressStr);
                XMLString::release(&sizeStr);
		XMLString::release(&memAccess);
                break;
            case load:
                memoryHandler->proc->getAddress(
                    stol(addrStr, nullptr, 16), atoi(size));
                XMLString::release(&address);
                XMLString::release(&size);
                XMLString::release(&addressStr);
                XMLString::release(&sizeStr);
		XMLString::release(&memAccess);
                break;
            default:
                XMLString::release(&address);
                XMLString::release(&size);
                XMLString::release(&addressStr);
                XMLString::release(&sizeStr);
		XMLString::release(&memAccess);
                break;
        }
    }

}

void SAX2Handler::fatalError(const SAXParseException& exception)
{
    char *message = XMLString::transcode(exception.getMessage());
	cout << "Fatal error: " << message << " at line ";
	cout << exception.getLineNumber() << endl;
	XMLString::release(&message);
}

