#pragma once

/*!
 * \file modbus_lmb_backend.h
 * \brief Libmodbus backend for gateway
 */

#include "modbus_wrapper.h"

#include <modbus/modbus.h>
#include <cstdlib>
#include <cerrno>

/*! Modbus TCP backend */
class TModbusTCPBackend : public IModbusBackend
{
public:
    TModbusTCPBackend(const char *hostname = "127.0.0.1", int port = 502)
    {
        char port_buffer[6]; // 5 dec symbols + \0
        std::snprintf(port_buffer, 6, "%u", port);
        _context = modbus_new_tcp_pi(hostname, port_buffer);

        if (!_context)
            throw ModbusException("can't allocate libmodbus context");

        queryBuffer = new uint8_t[MODBUS_TCP_MAX_ADU_LENGTH];
    }

    ~TModbusTCPBackend()
    {
        if (_mapping)
            modbus_mapping_free(_mapping);

        if (_context)
            modbus_free(_context);

        delete [] queryBuffer;
    }

    virtual void SetSlave(uint8_t slave_id)
    {
        slaveId = slave_id;
        if (modbus_set_slave(_context, slave_id))
            _error = errno;
    }

    virtual void Listen()
    {
        _s = modbus_tcp_pi_listen(_context, 1);
        if (modbus_tcp_pi_accept(_context, &_s))
            _error = errno;
    }

    virtual void AllocateCache(size_t di, size_t co, size_t ir, size_t hr)
    {
        _mapping = modbus_mapping_new(co, di, hr, ir);
        if (!_mapping)
            _error = errno;
    }

    virtual void *GetCache(TStoreType type)
    {
        if (!_mapping) {
            return nullptr; // TODO: error handling
        }

        switch (type) {
        case DISCRETE_INPUT:
            return _mapping->tab_input_bits;
        case COIL:
            return _mapping->tab_bits;
        case INPUT_REGISTER:
            return _mapping->tab_input_registers;
        case HOLDING_REGISTER:
            return _mapping->tab_registers;
        default:
            return nullptr; // TODO: error handling
        }
    }
    
    virtual uint8_t GetSlave()
    {
        return slaveId;
    }

    virtual TModbusQuery ReceiveQuery()
    {   
        int size = 0;
        do {
            size = modbus_receive(_context, queryBuffer);
        } while (size == 0);

        if (size < 0)
            return TModbusQuery::emptyQuery();

        return TModbusQuery(queryBuffer, size, modbus_get_header_length(_context));
    }

    virtual void Reply(const TModbusQuery &q)
    {
        if (q.size <= 0) 
            return;

        if (modbus_reply(_context, q.data, q.size, _mapping) < 0)
            _error = errno;
    }

    virtual void ReplyException(TReplyState e, const TModbusQuery &q)
    {
        unsigned code;

        switch (e) {
        case REPLY_ILLEGAL_FUNCTION:
            code = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            break;
        case REPLY_ILLEGAL_ADDRESS:
            code = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            break;
        case REPLY_ILLEGAL_VALUE:
            code = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            break;
        case REPLY_SERVER_FAILURE:
            code = MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE;
            break;
        default:
            return; // wtf
        }

        if (modbus_reply_exception(_context, q.data, code) == -1)
            _error = errno;
    }

    virtual int GetError()
    {
        return _error;
    }

protected:
    modbus_t *_context = nullptr;
    modbus_mapping_t *_mapping = nullptr;
    int _error = 0;
    int _s = -1;
    uint8_t slaveId = 0;
    uint8_t *queryBuffer = nullptr;
};
