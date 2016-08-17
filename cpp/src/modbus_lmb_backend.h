#pragma once

/*!
 * \file modbus_lmb_backend.h
 * \brief Libmodbus backend for gateway
 */

#include "modbus_wrapper.h"

#include <modbus/modbus.h>
#include <cstdlib>
#include <cerrno>
#include <error.h>
#include <queue>
#include <iostream>
#include <string>

#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

#include "logging.h"

// Maximum number of connections
#define NB_CONNECTIONS 2 


/*! Modbus TCP backend */
class TModbusTCPBackend : public IModbusBackend
{
public:
    TModbusTCPBackend(const char *hostname = "127.0.0.1", int port = 502)
        : _context(nullptr)
        , _error(0)
        , slaveId(0)
        , queryBuffer(nullptr)
        , fd_max(-1)
        , server_socket(-1)
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
        for (auto &p : _mappings)
            modbus_mapping_free(p.second);

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
        if (server_socket >= 0)
            throw ModbusException("No opened socket to listen");

        server_socket = modbus_tcp_pi_listen(_context, NB_CONNECTIONS);
        if (server_socket < 1)
            _error = errno;

        fd_max = server_socket;

        // configure select() stuff
        FD_ZERO(&refset);
        FD_SET(server_socket, &refset);

        LOG(INFO) << "Modbus listening";
    }

    virtual void AllocateCache(uint8_t slave_id, size_t di, size_t co, size_t ir, size_t hr)
    {
        if (_mappings[slave_id])
            return; // TODO: reallocations?

        _mappings[slave_id] = modbus_mapping_new(co, di, hr, ir);
        
        if (!_mappings[slave_id])
            _error = errno;
    }

    virtual void *GetCache(TStoreType type, uint8_t slave_id = 0)
    {
        if (!_mappings[slave_id]) {
            throw ModbusException(std::string("Cache for slave ID ") + std::to_string(slave_id) + " is not allocated");
        }

        switch (type) {
        case DISCRETE_INPUT:
            return _mappings[slave_id]->tab_input_bits;
        case COIL:
            return _mappings[slave_id]->tab_bits;
        case INPUT_REGISTER:
            return _mappings[slave_id]->tab_input_registers;
        case HOLDING_REGISTER:
            return _mappings[slave_id]->tab_registers;
        default:
            throw ModbusException("Unknown store type: " + std::to_string(type));
        }
    }
    
    virtual uint8_t GetSlave()
    {
        return slaveId;
    }

    virtual int WaitForMessages(int timeout = -1)
    {
        int num_msgs = 0;

        fd_set rdset = refset;

        struct timeval tv;
        tv.tv_usec = (timeout % 1000) * 1000;
        tv.tv_sec = timeout / 1000;

        if (select(fd_max + 1, &rdset, NULL, NULL, timeout == -1 ? NULL : &tv) == -1) {
            if (errno == EINTR)
                return 0; // just tell that no messages are available
            throw ModbusException(std::string("Error while select(): ") + strerror(errno));
        }

        // retrieve all available data into queue
        for (int s = 0; s <= fd_max; s++) {
            if (!FD_ISSET(s, &rdset))
                continue;

            if (s == server_socket) {  // accepting new connection
                struct sockaddr_in client;
                socklen_t addrlen = sizeof (client);
                memset(&client, 0, addrlen);

                int newfd = accept(server_socket, (struct sockaddr *) &client, &addrlen);
                if (newfd == -1) {
                    throw ModbusException(std::string("Error while accept(): ") + strerror(errno));
                }

                FD_SET(newfd, &refset);
                if (newfd > fd_max)
                    fd_max = newfd;

                LOG(DEBUG) << "Modbus incoming connection";
            } else { // receiving new query
                modbus_set_socket(_context, s);

                int rc = modbus_receive(_context, queryBuffer);
                if (rc > 0) {
                    QueuedQueries.push(TModbusQuery(queryBuffer, rc, modbus_get_header_length(_context), s));
                    num_msgs++;
                } else {
                    // TODO: error handling
                    LOG(DEBUG) << "Modbus closed connection";
                    close(s);
                    FD_CLR(s, &refset);

                    if (s == fd_max)
                        fd_max--;
                }
            }
        }

        return num_msgs;
    }

    virtual bool Available()
    {
        return !QueuedQueries.empty();
    }

    virtual TModbusQuery ReceiveQuery(bool block = false)
    {
        if (block && !Available())
            WaitForMessages(-1);
        
        if (Available()) {
            TModbusQuery q = QueuedQueries.front();
            QueuedQueries.pop();
            return q;
        } else {
            return TModbusQuery::emptyQuery();
        }
    }

    virtual void Reply(const TModbusQuery &q)
    {
        if (q.size <= 0) 
            return;

        modbus_set_socket(_context, q.socket_fd);

        if (modbus_reply(_context, q.data, q.size, _mappings[slaveId]) < 0)
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

    virtual void Close()
    {
        fd_max = -1;
    }

    virtual int GetError()
    {
        return _error;
    }

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
