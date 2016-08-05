#include "modbus_wrapper.h"

void TModbusServerBase::Observe(PModbusServerObserver o, StoreType store, const TModbusAddressRange &range)
{
    TModbusAddressRange *r;
    switch (store) {
    case DISCRETE_INPUT:
        r = &_di;
        break;
    case COIL:
        r = &_co;
        break;
    case INPUT_REGISTER:
        r = &_ir;
        break;
    case HOLDING_REGISTER:
        r = &_hr;
        break;
    default:
        return; // FIXME: error handling
    }

    r->insert(range, o);
}
