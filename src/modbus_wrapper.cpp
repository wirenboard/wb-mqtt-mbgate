#include "modbus_wrapper.h"
#include "log.h"

#include <map>

#define LOG(logger) ::logger.Log() << "[modbus] "

using namespace std;

TModbusServer::TModbusServer(PModbusBackend backend): mb(backend)
{
    // fill _CmdRangeMap for quick and pretty access in parser
    _CmdRangeMap[READ_COIL_STATUS] = &_co;
    _CmdRangeMap[READ_DISCRETE_INPUTS] = &_di;
    _CmdRangeMap[READ_HOLDING_REGISTERS] = &_hr;
    _CmdRangeMap[READ_INPUT_REGISTERS] = &_ir;

    _CmdRangeMap[FORCE_SINGLE_COIL] = &_co;
    _CmdRangeMap[PRESET_SINGLE_REGISTER] = &_hr;
    _CmdRangeMap[FORCE_MULTIPLE_COILS] = &_co;
    _CmdRangeMap[PRESET_MULTIPLE_REGISTERS] = &_hr;

    _CmdStoreTypeMap[READ_COIL_STATUS] = COIL;
    _CmdStoreTypeMap[READ_DISCRETE_INPUTS] = DISCRETE_INPUT;
    _CmdStoreTypeMap[READ_HOLDING_REGISTERS] = HOLDING_REGISTER;
    _CmdStoreTypeMap[READ_INPUT_REGISTERS] = INPUT_REGISTER;

    _CmdStoreTypeMap[FORCE_SINGLE_COIL] = COIL;
    _CmdStoreTypeMap[PRESET_SINGLE_REGISTER] = HOLDING_REGISTER;
    _CmdStoreTypeMap[FORCE_MULTIPLE_COILS] = COIL;
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

void TModbusServer::Observe(PModbusServerObserver o,
                            TStoreType store,
                            const TModbusAddressRange& range,
                            uint8_t slave_id)
{
    int offset = slave_id << 16;
    TRSet& max_addr = _maxSlaveAddresses[slave_id];

#define PROCESS(a, b)                                                                                                  \
    do {                                                                                                               \
        if (store & (a)) {                                                                                             \
            _##b.insert(range + offset, o);                                                                            \
            if (range.getEnd() > max_addr.b)                                                                           \
                max_addr.b = range.getEnd();                                                                           \
        }                                                                                                              \
    } while (0)

    PROCESS(DISCRETE_INPUT, di);
    PROCESS(COIL, co);
    PROCESS(INPUT_REGISTER, ir);
    PROCESS(HOLDING_REGISTER, hr);

#undef PROCESS
}

bool TModbusServer::IsObserved(uint8_t slave_id) const
{
    return _maxSlaveAddresses.find(slave_id) != _maxSlaveAddresses.end();
}

static void _callCacheAllocate(const TModbusAddressRange& range, uint8_t slave_id, TStoreType store, void* cache_start)
{
    map<PModbusServerObserver, TModbusCacheAddressRange> observers;

    // collect ranges for observers
    for (auto item = range.cbegin(); item != range.cend(); ++item) {
        int cache_offset = item->first;
        void* cache_ptr = cache_start;

        if (((cache_offset >> 16) & 0xFF) != slave_id)
            continue;
        else
            cache_offset &= 0xFFFF;

        // shift data offset for 16-bit values
        if (store == INPUT_REGISTER || store == HOLDING_REGISTER)
            cache_ptr = static_cast<uint16_t*>(cache_ptr) + cache_offset;
        else
            cache_ptr = static_cast<uint8_t*>(cache_ptr) + cache_offset;

        observers[item->second.second].insert(cache_offset, item->second.first - item->first, cache_ptr);
    }

    // call OnCacheAllocate() for each observer
    for (auto& item: observers) {
        item.first->OnCacheAllocate(store, slave_id, item.second);
    }
}

void TModbusServer::AllocateCache()
{
    for (auto& s: _maxSlaveAddresses) {
        const int slave_id = s.first;
        const TRSet& r = s.second;

        // allocate modbus mapping
        mb->AllocateCache(slave_id, r.di, r.co, r.ir, r.hr);

        // call OnCacheAllocate with correct ranges for each observer
        _callCacheAllocate(_di, slave_id, DISCRETE_INPUT, mb->GetCache(DISCRETE_INPUT, slave_id));
        _callCacheAllocate(_co, slave_id, COIL, mb->GetCache(COIL, slave_id));
        _callCacheAllocate(_ir, slave_id, INPUT_REGISTER, mb->GetCache(INPUT_REGISTER, slave_id));
        _callCacheAllocate(_hr, slave_id, HOLDING_REGISTER, mb->GetCache(HOLDING_REGISTER, slave_id));
    }

    LOG(Debug) << "Modbus cache allocated";
}

int TModbusServer::Loop(int timeoutMilliS)
{
    int rc = mb->WaitForMessages(timeoutMilliS);
    if (rc == -1) {
        LOG(Error) << mb->GetStrError();
        return -1;
    }

    // receive message, process, run callback
    while (mb->Available()) {
        TModbusQuery q = mb->ReceiveQuery();
        auto slave_id = q.header_length > 0 ? q.data[q.header_length - 1] : 0;
        if (q.size > 0 && IsObserved(slave_id)) {
            _ProcessQuery(q);
        }
    }

    return 0;
}

void TModbusServer::_ProcessQuery(const TModbusQuery& query)
{
    // get command code
    Command command = static_cast<Command>(query.data[query.header_length]);

    // get register address
    uint16_t start_address = _ReadU16(&(query.data[query.header_length + 1]));
    uint8_t slave_id = 0;

    // get slave ID and append it to address
    if (query.header_length > 0) {
        slave_id = query.data[query.header_length - 1];
    }

    uint16_t count;

    // get command data - address range and access mode
    if (_IsReadCmd(command)) {
        count = _ReadU16(&(query.data[query.header_length + 3]));
        _ProcessReadQuery(_CmdStoreTypeMap[command], *_CmdRangeMap[command], slave_id, start_address, count, query);
    } else {
        if (_IsSingleWriteCmd(command)) {
            count = 1;
        } else {
            count = _ReadU16(&(query.data[query.header_length + 3]));
        }

        // get values from write request to run pre-write action
        void* values;

        if (_IsCoilWriteCmd(command)) {
            uint8_t* raw_data = &(query.data[query.header_length + (_IsSingleWriteCmd(command) ? 3 : 6)]);
            uint8_t* int_values = new uint8_t[count];

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
            uint8_t* raw_data = &(query.data[query.header_length + (_IsSingleWriteCmd(command) ? 3 : 6)]);
            uint16_t* int_values = new uint16_t[count];

            for (int i = 0; i < count; i++)
                int_values[i] = (raw_data[2 * i] << 8) | (raw_data[2 * i + 1]);

            values = int_values;
        }

        _ProcessWriteQuery(_CmdStoreTypeMap[command],
                           *_CmdRangeMap[command],
                           slave_id,
                           start_address,
                           count,
                           query,
                           values);

        if (_IsCoilWriteCmd(command)) {
            delete[] static_cast<uint8_t*>(values);
        } else {
            delete[] static_cast<uint16_t*>(values);
        }
    }
}

void TModbusServer::_ProcessReadQuery(TStoreType type,
                                      TModbusAddressRange& range,
                                      uint8_t slave_id,
                                      int start,
                                      unsigned count,
                                      const TModbusQuery& query)
{
    // ask callback, then reply
    try {
        void* cache_ptr;
        int item_size;

        try {
            if (type == COIL || type == DISCRETE_INPUT) {
                cache_ptr = static_cast<uint8_t*>(mb->GetCache(type, slave_id)) + start;
                item_size = sizeof(uint8_t);
            } else {
                cache_ptr = static_cast<uint16_t*>(mb->GetCache(type, slave_id)) + start;
                item_size = sizeof(uint16_t);
            }
        } catch (const TModbusException& e) {
            mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
            return;
        }

        int slave_offset = slave_id << 16;

        auto segments = range.getSegments(start + slave_offset,
                                          count); //->OnGetValue(type, mb->GetSlave(), start, count, cache_ptr);
        TReplyState reply = REPLY_ILLEGAL_ADDRESS;

        for (auto& s: segments) {
            const int count = s.getCount();
            reply = s.getParam()->OnGetValue(type, slave_id, s.getStart() - slave_offset, count, cache_ptr);

            if (item_size == sizeof(uint16_t)) {
                cache_ptr = static_cast<uint16_t*>(cache_ptr) + count;
            } else {
                cache_ptr = static_cast<uint8_t*>(cache_ptr) + count;
            }

            if (reply > 0)
                break;
        }

        if (reply <= 0)
            mb->Reply(query);
        else
            mb->ReplyException(reply, query);
    } catch (const WrongSegmentException& e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    }
}

void TModbusServer::_ProcessWriteQuery(TStoreType type,
                                       TModbusAddressRange& range,
                                       uint8_t slave_id,
                                       int start,
                                       unsigned count,
                                       const TModbusQuery& query,
                                       const void* data_ptr)
{
    // reply then ask callback (modbus cache will contain required value)
    PModbusServerObserver obs;

    try {
        int item_size;

        if (type == COIL || type == DISCRETE_INPUT) {
            item_size = sizeof(uint8_t);
        } else {
            item_size = sizeof(uint16_t);
        }

        int slave_offset = slave_id << 16;

        auto segments = range.getSegments(start + slave_offset, count);

        TReplyState reply = REPLY_ILLEGAL_ADDRESS;

        for (auto& s: segments) {
            const int count = s.getCount();
            reply = s.getParam()->OnSetValue(type, slave_id, s.getStart() - slave_offset, count, data_ptr);

            if (item_size == sizeof(uint16_t)) {
                data_ptr = static_cast<const uint16_t*>(data_ptr) + count;
            } else {
                data_ptr = static_cast<const uint8_t*>(data_ptr) + count;
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
    } catch (const WrongSegmentException& e) {
        mb->ReplyException(TReplyState::REPLY_ILLEGAL_ADDRESS, query);
    } catch (const int&) {
        // dummy, just to get away
    }
}
