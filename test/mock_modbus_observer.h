#pragma once

#include "modbus_wrapper.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

/*! Mock'ed Modbus observer */
class MockModbusServerObserver: public IModbusServerObserver
{
public:
    virtual ~MockModbusServerObserver();

    MOCK_METHOD5(OnGetValue, TReplyState(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void* data));
    MOCK_METHOD5(OnSetValue,
                 TReplyState(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void* data));
    MOCK_METHOD3(OnCacheAllocate, void(TStoreType type, uint8_t unit_id, const TModbusCacheAddressRange& cache));
};
