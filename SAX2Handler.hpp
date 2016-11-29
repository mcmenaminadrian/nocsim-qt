#ifndef __SAX2_HANDLER_
#define __SAX2_HANDLER_

#include <xercesc/sax2/DefaultHandler.hpp>

class SAX2Handler: public DefaultHandler {

	private:
		XMLFunctor *memoryHandler;

	public:
		setMemoryHandler(XMLFunctor *handler);
		void startElement(
			const XMLCh* const	uri,
			const XMLCh* const	localname,
			const XMLCh* const	qname,
			const Attributes&	attrs
		};
		void fatalError(const SAXParserException&);
};


#endif
