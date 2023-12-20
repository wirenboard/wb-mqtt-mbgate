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

/*! Modbus base backend */
class TModbusBaseBackend: public IModbusBackend
{
public:
    TModbusBaseBackend();
    ~TModbusBaseBackend();

    void SetSlave(uint8_t slave_id) override;
    void AllocateCache(uint8_t slave_id, size_t di, size_t co, size_t ir, size_t hr) override;
    void* GetCache(TStoreType type, uint8_t slave_id = 0) override;
    uint8_t GetSlave() override;
    void SetDebug(bool debug) override;
    bool Available() override;
    void Reply(const TModbusQuery& q) override;
    void ReplyException(TReplyState e, const TModbusQuery& q) override;
    int GetError() override;
    const char* GetStrError() override;
    TModbusQuery ReceiveQuery(bool block = false) override;

protected:
    virtual void PreReply(const TModbusQuery& q) = 0;
    virtual void PostReply(const TModbusQuery& q) = 0;

    modbus_t* _context;
    std::map<uint8_t, modbus_mapping_t*> _mappings;
    int _error;
    uint8_t slaveId;
    uint8_t* queryBuffer;

    std::queue<TModbusQuery> QueuedQueries;

    fd_set refset;
};

/*! Modbus TCP backend */
class TModbusTCPBackend: public TModbusBaseBackend
{
    using Base = TModbusBaseBackend;

public:
    TModbusTCPBackend(const char* hostname = "127.0.0.1", int port = 502);

    void Listen() override;
    int WaitForMessages(int timeout = -1) override;
    void Close() override;

private:
    void PreReply(const TModbusQuery& q) override;
    void PostReply(const TModbusQuery& q) override;

    int server_socket;
    int fd_max;
};

struct TModbusRTUBackendArgs
{
    std::string Device;
    int BaudRate = 9600;
    char Parity = 'N';
    int DataBits = 8;
    int StopBits = 1;
};

/*! Modbus RTU backend */
class TModbusRTUBackend: public TModbusBaseBackend
{
    using Base = TModbusBaseBackend;

public:
    TModbusRTUBackend(const TModbusRTUBackendArgs& args);

    void Listen() override;
    int WaitForMessages(int timeout = -1) override;
    void Close() override;

private:
    void PreReply(const TModbusQuery& q) override;
    void PostReply(const TModbusQuery& q) override;

    int fd;
};
