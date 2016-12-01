#include <iostream>
#include <vector>
#include "memorypacket.hpp"
#include "mux.hpp"
#include "ControlThread.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"


using namespace std;
using namespace xercesc;


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

	//test code
	char *message = XMLString::transcode(localname);
	cout << "Element: " << message << endl;
	XMLString::release(&message);

}

void SAX2Handler::fatalError(const SAXParseException& exception)
{
    char *message = XMLString::transcode(exception.getMessage());
	cout << "Fatal error: " << message << " at line ";
	cout << exception.getLineNumber() << endl;
	XMLString::release(&message);
}

