/* PvaClientNTMultiPut.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.03
 */

#define epicsExportSharedSymbols
#include <pv/pvaClientMultiChannel.h>
#include <pv/standardField.h>
#include <pv/convert.h>
#include <epicsMath.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::nt;
using namespace std;

namespace epics { namespace pvaClient { 

static ConvertPtr convert = getConvert();
static FieldCreatePtr fieldCreate = getFieldCreate();
static PVDataCreatePtr pvDataCreate = getPVDataCreate();

PvaClientNTMultiPutPtr PvaClientNTMultiPut::create(
    PvaClientMultiChannelPtr const &pvaMultiChannel,
    PvaClientChannelArray const &pvaClientChannelArray)
{
    return PvaClientNTMultiPutPtr(
        new PvaClientNTMultiPut(pvaMultiChannel,pvaClientChannelArray));
}

PvaClientNTMultiPut::PvaClientNTMultiPut(
         PvaClientMultiChannelPtr const &pvaClientMultiChannel,
         PvaClientChannelArray const &pvaClientChannelArray)
: pvaClientMultiChannel(pvaClientMultiChannel),
  pvaClientChannelArray(pvaClientChannelArray),
  nchannel(pvaClientChannelArray.size()),
  unionValue(shared_vector<epics::pvData::PVUnionPtr>(nchannel,PVUnionPtr())),
  value(shared_vector<epics::pvData::PVFieldPtr>(nchannel,PVFieldPtr())),
  isConnected(false),
  isDestroyed(false)
{
}


PvaClientNTMultiPut::~PvaClientNTMultiPut()
{
    destroy();
}

void PvaClientNTMultiPut::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    pvaClientChannelArray.clear();
}

void PvaClientNTMultiPut::connect()
{
    pvaClientPut.resize(nchannel);
    shared_vector<epics::pvData::boolean> isConnected = pvaClientMultiChannel->getIsConnected();
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               pvaClientPut[i] = pvaClientChannelArray[i]->createPut();
               pvaClientPut[i]->issueConnect();
         }
    }
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               Status status = pvaClientPut[i]->waitConnect();
               if(status.isOK()) continue;
               string message = string("channel ") +pvaClientChannelArray[i]->getChannelName() 
                    + " PvaChannelPut::waitConnect " + status.getMessage();
               throw std::runtime_error(message);
         }
    }
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
              pvaClientPut[i]->issueGet();
         }
    }
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               Status status = pvaClientPut[i]->waitGet();
               if(status.isOK()) continue;
               string message = string("channel ") +pvaClientChannelArray[i]->getChannelName() 
                    + " PvaChannelPut::waitGet " + status.getMessage();
               throw std::runtime_error(message);
         }
    }
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
             value[i] = pvaClientPut[i]->getData()->getValue();
             FieldBuilderPtr builder = fieldCreate->createFieldBuilder();
             builder->add("value",value[i]->getField());
             unionValue[i] = pvDataCreate->createPVUnion(builder->createUnion());
         }
    }
    this->isConnected = true;
}

shared_vector<epics::pvData::PVUnionPtr> PvaClientNTMultiPut::getValues()
{
    if(!isConnected) connect();
    return unionValue;
}

void PvaClientNTMultiPut::put()
{
    if(!isConnected) connect();
    shared_vector<epics::pvData::boolean> isConnected = pvaClientMultiChannel->getIsConnected();
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               value[i]->copy(*unionValue[i]->get());
               pvaClientPut[i]->issuePut();
         }
         if(isConnected[i]) {
              Status status = pvaClientPut[i]->waitPut();
              if(status.isOK())  continue;
              string message = string("channel ") +pvaClientChannelArray[i]->getChannelName() 
                    + " PvaChannelPut::waitPut " + status.getMessage();
              throw std::runtime_error(message); 
         }
    }
}


}}
