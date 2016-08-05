#pragma once

#include <memory>
#include <list>
#include <cstdint>

#include <modbus/modbus.h>
#include "address_range.h"

/*! Modbus store types */
enum StoreType {
    DISCRETE_INPUT = 1,
    COIL = 2,
    INPUT_REGISTER = 4,
    HOLDING_REGISTER = 8
};


/*!
 * \brief Modbus server observer interface
 * Modbus server observer is able to reply on Modbus' READ_*, WRITE_* for coils and registers.
 */
class IModbusServerObserver 
{

    /*! Return values for OnGetValue and OnSetValue callbacks */
    enum ReplyState {
        REPLY_CACHED = -1,  /*!< Don't use observer, use cached value instead */
        REPLY_OK = 0, /*!< Reply is correct */
        
        REPLY_ILLEGAL_ADDRESS = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                /*!< Wrong address for this datablock */
        REPLY_ILLEGAL_VALUE = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                /*!< Wrong value given for this datablock */
        REPLY_FAILURE = MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE,
                /*!< Server failure */
    };

public:
    /* Virtual destructor */
    virtual ~IModbusServerObserver();

    /*! Callback for READ_* functions
     * May be ignored if observer stores cached value in server context
     * \param type      Type of store
     * \param unit_id   Modbus TCP unit ID (or ID of serial device in case of RTU)
     * \param start     First element address
     * \param count     Number of elements required
     * \param data      Pointer to request data
     * \return Reply state
     */
    virtual ReplyState OnGetValue(StoreType type, uint8_t unit_id, uint16_t start, int count, const uint8_t *data);
    
    /*! Callback for WRITE_* functions
     * May be ignored if observer stores cached value in server context
     * \param type      Type of store
     * \param unit_id   Modbus TCP unit ID (or ID of serial device in case of RTU)
     * \param start     First element address
     * \param count     Number of elements required
     * \param data      Pointer to request data
     * \return Reply state
     */
    virtual ReplyState OnSetValue(StoreType type, uint8_t unit_id, uint16_t start, int count, const uint8_t *data);
};

/*! Shared pointer to IModbusObserver */
typedef std::shared_ptr<IModbusServerObserver> PModbusServerObserver;

/*! Definition of Modbus address range */
typedef TAddressRange<PModbusServerObserver> TModbusAddressRange;
typedef std::shared_ptr<TModbusAddressRange> PModbusAddressRange;


/*!
 * \brief Modbus server wrapper base class
 */
class TModbusServerBase
{
public:
    /* Virtual destructor */
    virtual ~TModbusServerBase();

    /*! Modbus main loop function
     */
    virtual void Loop() = 0;

    /*! Register observer for given store type and range
     * \param o Pointer to observer object
     * \param store Store type (member of StoreType)
     * \param range Address range to handle
     */
    virtual void Observe(PModbusServerObserver o, StoreType store, const TModbusAddressRange &range);
    /* virtual void Unobserve(PModbusServerObserver o) = 0; */

protected:
    /*! Modbus address ranges */
    TModbusAddressRange _di, _co, _ir, _hr;
};

/*! Shared pointer to TModbusServerBase */
typedef std::shared_ptr<TModbusServerBase> PModbusServerBase;
