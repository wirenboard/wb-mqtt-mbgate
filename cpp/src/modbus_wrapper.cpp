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

void TModbusServer::Backend(PModbusBackend backend)
{
    mb = backend;
}

PModbusBackend TModbusServer::Backend()
{
    return mb;
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

int TModbusServer::Loop(int timeout)
{
    if (mb->WaitForMessages(timeout) == -1)
        return -1;

    // receive message, process, run callback
    while (mb->Available()) {
        TModbusQuery q = mb->ReceiveQuery();
        if (q.size > 0)
            _ProcessQuery(q);
    }

    return 0;
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
    } else {
        if (_IsSingleWriteCmd(command)) {
            count = 1;
        } else {
            count = _ReadU16(&(query.data[query.header_length + 3]));
        }

        void *values;

        if (_IsCoilWriteCmd(command)) {
            uint8_t *raw_data = &(query.data[query.header_length + (_IsSingleWriteCmd(command) ? 3 : 6)]);
            uint8_t *int_values = new uint8_t[count];

            uint8_t bits = 1;
            int q = 0;

            for (int i = 0; i < count; i++) {
                int_values[i] = raw_data[q] & bits ? 0xFF : 0;
                bits <<= 1;
                if (bits == 0) {
                    q++;
                    bits = 1;
                }
            }

            values = int_values;
        } else {
            uint8_t *raw_data = &(query.data[query.header_length + (_IsSingleWriteCmd(command) ? 3 : 6)]);
            uint16_t *int_values = new uint16_t[count];

            for (int i = 0; i < count; i++)
                int_values[i] = (raw_data[2 * i] << 8) | (raw_data[2 * i + 1]);

            values = int_values;
        }
        
        _ProcessWriteQuery(_CmdStoreTypeMap[command], *_CmdRangeMap[command], start_address, count, query, values);

        if (_IsCoilWriteCmd(command)) {
            delete [] static_cast<uint8_t*>(values);
        } else {
            delete [] static_cast<uint16_t *>(values);
        }
    }
}

void TModbusServer::_ProcessReadQuery(TStoreType type, TModbusAddressRange &range, int start, unsigned count, const TModbusQuery &query)
{
    // ask callback, then reply
    try {
        void *cache_ptr;
        int item_size;

        if (type == COIL || type == DISCRETE_INPUT) {
            cache_ptr = static_cast<uint8_t *>(mb->GetCache(type)) + start;
            item_size = sizeof (uint8_t);
        } else {
            cache_ptr = static_cast<uint16_t *>(mb->GetCache(type)) + start;
            item_size = sizeof (uint16_t);
        }

        auto segments = range.getSegments(start, count);//->OnGetValue(type, mb->GetSlave(), start, count, cache_ptr);
        TReplyState reply = REPLY_ILLEGAL_ADDRESS;

        for (auto &s : segments) {
            const int count = s.getCount();
            reply = s.getParam()->OnGetValue(type, mb->GetSlave(), s.getStart(), count, cache_ptr);
            
            if (item_size == sizeof (uint16_t)) {
                cache_ptr = static_cast<uint16_t *>(cache_ptr) + count;
            } else {
                cache_ptr = static_cast<uint8_t *>(cache_ptr) + count;
            }

            if (reply > 0)
                break;
        }
        
        if (reply <= 0)
            mb->Reply(query);
        else
            mb->ReplyException(reply, query);
    } catch (const WrongSegmentException &e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    }
}

void TModbusServer::_ProcessWriteQuery(TStoreType type, TModbusAddressRange &range, int start, unsigned count, const TModbusQuery &query, const void *data_ptr)
{
    // reply then ask callback (modbus cache will contain required value)
    PModbusServerObserver obs;

    try {
        int item_size;

        if (type == COIL || type == DISCRETE_INPUT) {
            item_size = sizeof (uint8_t);
        } else {
            item_size = sizeof (uint16_t);
        }

        auto segments = range.getSegments(start, count);
        
        TReplyState reply = REPLY_ILLEGAL_ADDRESS;

        for (auto &s : segments) {
            const int count = s.getCount();
            reply = s.getParam()->OnSetValue(type, mb->GetSlave(), s.getStart(), count, data_ptr);
            
            if (item_size == sizeof (uint16_t)) {
                data_ptr = static_cast<const uint16_t *>(data_ptr) + count;
            } else {
                data_ptr = static_cast<const uint8_t *>(data_ptr) + count;
            }

            if (reply > 0)
                break;
        }
        
        if (reply <= 0) {
            mb->Reply(query);
        } else {
            mb->ReplyException(reply, query);
            throw 10; // just to exit from try {} block
        }
    } catch (const WrongSegmentException &e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    } catch (const int &) {
        // dummy, just to get away
    }
}
