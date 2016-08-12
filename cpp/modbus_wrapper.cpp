#include "modbus_wrapper.h"

#include <map>

using namespace std;

TModbusServer::TModbusServer(PModbusBackend backend)
    : mb(backend)
{
    // fill _CmdRangeMap for quick and pretty access in parser
    _CmdRangeMap[READ_COIL_STATUS]              = &_co;
    _CmdRangeMap[READ_DISCRETE_INPUTS]          = &_di;
    _CmdRangeMap[READ_HOLDING_REGISTERS]        = &_hr;
    _CmdRangeMap[READ_INPUT_REGISTERS]          = &_ir;

    _CmdRangeMap[FORCE_SINGLE_COIL]             = &_co;
    _CmdRangeMap[PRESET_SINGLE_REGISTER]        = &_hr;
    _CmdRangeMap[FORCE_MULTIPLE_COILS]          = &_co;
    _CmdRangeMap[PRESET_MULTIPLE_REGISTERS]     = &_hr;

    _CmdStoreTypeMap[READ_COIL_STATUS]          = COIL;
    _CmdStoreTypeMap[READ_DISCRETE_INPUTS]      = DISCRETE_INPUT;
    _CmdStoreTypeMap[READ_HOLDING_REGISTERS]    = HOLDING_REGISTER;
    _CmdStoreTypeMap[READ_INPUT_REGISTERS]      = INPUT_REGISTER;

    _CmdStoreTypeMap[FORCE_SINGLE_COIL]         = COIL;
    _CmdStoreTypeMap[PRESET_SINGLE_REGISTER]    = HOLDING_REGISTER;
    _CmdStoreTypeMap[FORCE_MULTIPLE_COILS]      = COIL;
    _CmdStoreTypeMap[PRESET_MULTIPLE_REGISTERS] = HOLDING_REGISTER;
}

void TModbusServer::Observe(PModbusServerObserver o, TStoreType store, const TModbusAddressRange &range)
{
    if (store & DISCRETE_INPUT)
        _di.insert(range, o);
    if (store & COIL)
        _co.insert(range, o);
    if (store & INPUT_REGISTER)
        _ir.insert(range, o);
    if (store & HOLDING_REGISTER)
        _hr.insert(range, o);
}

// Helpers for AllocateCache()
static int _getMaxAddress(const TModbusAddressRange &range)
{
    int max = 0;
    for (auto item = range.cbegin(); item != range.cend(); ++item)
        if (item->second.first > max)
            max = item->second.first;

    return max;
}

static void _callCacheAllocate(const TModbusAddressRange &range, TStoreType store, void *cache_start)
{
    map<PModbusServerObserver, TModbusCacheAddressRange> observers;

    // collect ranges for observers
    for (auto item = range.cbegin(); item != range.cend(); ++item) {
        int cache_offset = item->first;
        void *cache_ptr = cache_start;

        // shift data offset for 16-bit values
        if (store == INPUT_REGISTER || store == HOLDING_REGISTER)
            cache_ptr = static_cast<uint16_t *>(cache_ptr) + cache_offset;
        else
            cache_ptr = static_cast<uint8_t *>(cache_ptr) + cache_offset;


        observers[item->second.second].insert(item->first, item->second.first - item->first, cache_ptr);
    }

    // call OnCacheAllocate() for each observer
    for (auto &item: observers) {
        item.first->OnCacheAllocate(store, item.second);
    }
}

void TModbusServer::AllocateCache()
{
    // get maximum address for each register type
    int max_di = _getMaxAddress(_di);
    int max_co = _getMaxAddress(_co);
    int max_ir = _getMaxAddress(_ir);
    int max_hr = _getMaxAddress(_hr);

    // allocate modbus mapping
    mb->AllocateCache(max_di, max_co, max_ir, max_hr);

    // call OnCacheAllocate with correct ranges for each observer
    _callCacheAllocate(_di, DISCRETE_INPUT, mb->GetCache(DISCRETE_INPUT));
    _callCacheAllocate(_co, COIL, mb->GetCache(COIL));
    _callCacheAllocate(_ir, INPUT_REGISTER, mb->GetCache(INPUT_REGISTER));
    _callCacheAllocate(_hr, HOLDING_REGISTER, mb->GetCache(HOLDING_REGISTER));
}

void TModbusServer::Loop(int timeout)
{
    mb->WaitForMessages(timeout);
    // receive message, process, run callback
    while (mb->Available()) {
        TModbusQuery q = mb->ReceiveQuery();
        if (q.size > 0)
            _ProcessQuery(q);
    }
}

void TModbusServer::_ProcessQuery(const TModbusQuery &query)
{
    // get command code
    Command command = static_cast<Command>(query.data[query.header_length]);

    uint16_t start_address = _ReadU16(&(query.data[query.header_length + 1]));
    uint16_t count;

    // get command data - address range and access mode
    if (_IsReadCmd(command)) {
        count = _ReadU16(&(query.data[query.header_length + 3]));
        _ProcessReadQuery(_CmdStoreTypeMap[command], *_CmdRangeMap[command], start_address, count, query);
    } else if (_IsSingleWriteCmd(command)) {
        _ProcessWriteQuery(_CmdStoreTypeMap[command], *_CmdRangeMap[command], start_address, 1, query);
    } else if (_IsMultiWriteCmd(command)) {
        count = _ReadU16(&(query.data[query.header_length + 3]));
        _ProcessWriteQuery(_CmdStoreTypeMap[command], *_CmdRangeMap[command], start_address, count, query);
    }
}

void TModbusServer::_ProcessReadQuery(TStoreType type, TModbusAddressRange &range, int start, unsigned count, const TModbusQuery &query)
{
    // ask callback, then reply
    try {
        void *cache_ptr;
        if (type == COIL || type == DISCRETE_INPUT)
            cache_ptr = static_cast<uint8_t *>(mb->GetCache(type)) + start;
        else
            cache_ptr = static_cast<uint16_t *>(mb->GetCache(type)) + start;

        TReplyState ret = range.getParam(start, count)->OnGetValue(type, mb->GetSlave(), start, count, cache_ptr);
        
        if (ret <= 0)
            mb->Reply(query);
        else
            mb->ReplyException(ret, query);
    } catch (const WrongSegmentException &e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    }
}

void TModbusServer::_ProcessWriteQuery(TStoreType type, TModbusAddressRange &range, int start, unsigned count, const TModbusQuery &query)
{
    // reply then ask callback (modbus cache will contain required value)
    PModbusServerObserver obs;

    try {
        obs = range.getParam(start, count);
    } catch (const WrongSegmentException &e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    }
    
    mb->Reply(query);

    void *cache_ptr;
    if (type == COIL || type == DISCRETE_INPUT)
        cache_ptr = static_cast<uint8_t *>(mb->GetCache(type)) + start;
    else
        cache_ptr = static_cast<uint16_t *>(mb->GetCache(type)) + start;

    obs->OnSetValue(type, mb->GetSlave(), start, count, cache_ptr);
}
