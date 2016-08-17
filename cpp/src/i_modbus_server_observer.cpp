#include "modbus_wrapper.h"

TReplyState IModbusServerObserver::OnGetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void *data)
{
    return TReplyState::REPLY_CACHED;
}

TReplyState IModbusServerObserver::OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data)
{
    return TReplyState::REPLY_OK;
}

void IModbusServerObserver::OnCacheAllocate(TStoreType type, uint8_t unit_id, const TModbusCacheAddressRange& cache)
{

}

IModbusServerObserver::~IModbusServerObserver()
{

}
