#pragma once

/*!
 * \file modbus_lmb_backend.h
 * \brief Libmodbus backend for gateway
 */

#include "modbus_wrapper.h"

#include <queue>

#include <modbus/modbus.h>

// Maximum number of connections
#define NB_CONNECTIONS 2 


/*! Modbus TCP backend */
class TModbusTCPBackend : public IModbusBackend
{
public:
    TModbusTCPBackend(const std::string& hostname = "127.0.0.1", int port = 502);
    ~TModbusTCPBackend();

    virtual void SetSlave(uint8_t slave_id);

    virtual void Listen();

    virtual void AllocateCache(uint8_t slave_id, size_t di, size_t co, size_t ir, size_t hr);

    virtual void *GetCache(TStoreType type, uint8_t slave_id = 0);
    
    virtual uint8_t GetSlave();

    virtual int WaitForMessages(int timeoutMilliS = -1);

    virtual bool Available();

    virtual TModbusQuery ReceiveQuery(bool block = false);

    virtual void Reply(const TModbusQuery &q);

    virtual void ReplyException(TReplyState e, const TModbusQuery &q);

    virtual void Close();

    virtual int GetError();

protected:
    modbus_t *_context;
    std::map<uint8_t, modbus_mapping_t *> _mappings;
    int _error;
    uint8_t slaveId;
    uint8_t *queryBuffer;

    std::queue<TModbusQuery> QueuedQueries;

private:
    fd_set refset;
    int fd_max;
    int server_socket;
};
