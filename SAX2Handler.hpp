#ifndef __SAX2_HANDLER_
#define __SAX2_HANDLER_

#include <xercesc/sax2/DefaultHandler.hpp>

using namespace xercesc;

class SAX2Handler: public xercesc::DefaultHandler {

	private:
		XMLFunctor *memoryHandler;

	public:
        SAX2Handler();
        void setMemoryHandler(XMLFunctor *handler);
		void startElement(
			const XMLCh* const	uri,
			const XMLCh* const	localname,
			const XMLCh* const	qname,
			const Attributes&	attrs
        );

        void fatalError(const SAXParseException&);
};


#endif
