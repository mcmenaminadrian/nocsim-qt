#include <iostream>
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"

using namespace std;

SAX2Handler::SAX2Handler()
{ 
	memoryHandler = nullptr;
}

void SAX2Handler::setMemoryHandler(XMLFunctor *handler)
{
	memoryHandler = hanlder;
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

void SAX2Handler::fatalError(const SAXParseException& execption)
{
	char *message = XMLString::transcode(exceotion.getMessage());
	cout << "Fatal error: " << message << " at line ";
	cout << exception.getLineNumber() << endl;
	XMLString::release(&message);
}

