/* pvaClientMultiMonitorDouble.cpp */
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
#include <epicsThread.h>
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
static StandardFieldPtr standardField = getStandardField();


PvaClientMultiMonitorDoublePtr PvaClientMultiMonitorDouble::create(
    PvaClientMultiChannelPtr const &pvaMultiChannel,
    PvaClientChannelArray const &pvaClientChannelArray)
{
    PvaClientMultiMonitorDoublePtr pvaClientMultiMonitorDouble(
         new PvaClientMultiMonitorDouble(pvaMultiChannel,pvaClientChannelArray));
    return pvaClientMultiMonitorDouble;
}

PvaClientMultiMonitorDouble::PvaClientMultiMonitorDouble(
     PvaClientMultiChannelPtr const &pvaClientMultiChannel,
     PvaClientChannelArray const &pvaClientChannelArray)
: pvaClientMultiChannel(pvaClientMultiChannel),
  pvaClientChannelArray(pvaClientChannelArray),
  nchannel(pvaClientChannelArray.size()),
  doubleValue(shared_vector<double>(nchannel,epicsNAN)),
  pvaClientMonitor(std::vector<PvaClientMonitorPtr>(nchannel,PvaClientMonitorPtr())),
  isMonitorConnected(false),
  isDestroyed(false)
{
}

PvaClientMultiMonitorDouble::~PvaClientMultiMonitorDouble()
{
    destroy();
}

void PvaClientMultiMonitorDouble::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    pvaClientChannelArray.clear();
}

void PvaClientMultiMonitorDouble::connect()
{
    shared_vector<epics::pvData::boolean> isConnected = pvaClientMultiChannel->getIsConnected();
    string request = "value";
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               pvaClientMonitor[i] = pvaClientChannelArray[i]->createMonitor(request);
               pvaClientMonitor[i]->issueConnect();
         }
    }
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
               Status status = pvaClientMonitor[i]->waitConnect();
               if(status.isOK()) continue;
               string message = string("channel ") + pvaClientChannelArray[i]->getChannelName()
                   + " PvaChannelMonitor::waitConnect " + status.getMessage();
               throw std::runtime_error(message);
         }
    }
     for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) pvaClientMonitor[i]->start();
    }
    isMonitorConnected = true;
}

bool PvaClientMultiMonitorDouble::poll()
{
    if(!isMonitorConnected){
         connect();
         epicsThreadSleep(.01);
    }
    bool result = false;
    shared_vector<epics::pvData::boolean> isConnected = pvaClientMultiChannel->getIsConnected();
    for(size_t i=0; i<nchannel; ++i)
    {
         if(isConnected[i]) {
              if(pvaClientMonitor[i]->poll()) {
                   doubleValue[i] = pvaClientMonitor[i]->getData()->getDouble();
                   pvaClientMonitor[i]->releaseEvent();
                   result = true;
              }
         }
    }
    return result;
}

bool PvaClientMultiMonitorDouble::waitEvent(double waitForEvent)
{
    if(poll()) return true;
    TimeStamp start;
    start.getCurrent();
    TimeStamp now;
    while(true) {
          epicsThreadSleep(.1);
          if(poll()) return true;
          now.getCurrent();
          double diff = TimeStamp::diff(now,start);
          if(diff>=waitForEvent) break;
    }
    return false;
}

epics::pvData::shared_vector<double> PvaClientMultiMonitorDouble::get()
{
    return doubleValue;
}


}}
