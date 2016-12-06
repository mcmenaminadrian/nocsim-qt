#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include <map>
#include <QFile>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/util/XMLString.hpp>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "noc.hpp"
#include "memory.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"

using namespace std;
using namespace xercesc;

const uint64_t XMLFunctor::sumCount = 0x101;


//avoid magic numbers


#define SETSIZE 8

XMLFunctor::XMLFunctor(Tile *tileIn):
	tile{tileIn}, proc{tileIn->tileProcessor}
{ }


void XMLFunctor::operator()()
{
    const uint64_t order = tile->getOrder();
    if (order >= SETSIZE) {
        	return;
    }
    proc->start();
	
    string lackeyml("lackeyml_");
	lackeyml += to_string(tile->getOrder());

	SAX2XMLReader *parser = XMLReaderFactory::createXMLReader();
    parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
    parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);

    SAX2Handler *lackeyHandler = new SAX2Handler();
    parser->setContentHandler(lackeyHandler);
    parser->setErrorHandler(lackeyHandler);
    lackeyHandler->setMemoryHandler(this);

    parser->parse(lackeyml.c_str());
    cout << "Task on " << order << " completed." << endl;
    cout << "Hard fault count: " << proc->hardFaultCount << endl;
    cout << "Small fault count: " << proc->smallFaultCount << endl;
    cout << "Ticks: " << proc->getTicks() << endl;
    proc->getTile()->getBarrier()->decrementTaskCount();
}
