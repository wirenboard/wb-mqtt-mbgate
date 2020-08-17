#include "log.h"

#include "modbus_lmb_backend.h"

#include <cstdlib>
#include <cerrno>
#include <error.h>
#include <iostream>
#include <string>

#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>


#define LOG(logger) ::logger.Log() << "[modbus tcp] "


TModbusTCPBackend::TModbusTCPBackend(const std::string& hostname, int port)
    : _context(nullptr)
    , _error(0)
    , slaveId(0)
    , queryBuffer(nullptr)
    , fd_max(-1)
    , server_socket(-1)
{
    char port_buffer[6]; // 5 dec symbols + \0
    std::snprintf(port_buffer, 6, "%u", port);
    _context = modbus_new_tcp_pi(hostname.c_str(), port_buffer);

    if (!_context)
        throw ModbusException("can't allocate libmodbus context");

    queryBuffer = new uint8_t[MODBUS_TCP_MAX_ADU_LENGTH];
}

TModbusTCPBackend::~TModbusTCPBackend()
{
    for (auto &p : _mappings)
        modbus_mapping_free(p.second);

    if (_context)
        modbus_free(_context);

    delete [] queryBuffer;
}

void TModbusTCPBackend::SetSlave(uint8_t slave_id)
{
    slaveId = slave_id;
    if (modbus_set_slave(_context, slave_id))
        _error = errno;
}

void TModbusTCPBackend::Listen()
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

    LOG(Info) << "Modbus listening";
}

void TModbusTCPBackend::AllocateCache(uint8_t slave_id, size_t di, size_t co, size_t ir, size_t hr)
{
    if (_mappings[slave_id])
        return; // TODO: reallocations?

    _mappings[slave_id] = modbus_mapping_new(co, di, hr, ir);
    
    if (!_mappings[slave_id])
        _error = errno;
}

void* TModbusTCPBackend::GetCache(TStoreType type, uint8_t slave_id)
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

uint8_t TModbusTCPBackend::GetSlave()
{
    return slaveId;
}

int TModbusTCPBackend::WaitForMessages(int timeoutMilliS)
{
    int num_msgs = 0;

    fd_set rdset = refset;

    struct timeval tv;
    tv.tv_usec = (timeoutMilliS % 1000) * 1000;
    tv.tv_sec = timeoutMilliS / 1000;

    int res = select(fd_max + 1, &rdset, NULL, NULL, timeoutMilliS == -1 ? NULL : &tv);
    if (res == 0) {
        return 0; // just tell that no messages are available
    }

    if (res == -1) {
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

            LOG(Debug) << "Modbus incoming connection";
        } else { // receiving new query
            modbus_set_socket(_context, s);

            int rc = modbus_receive(_context, queryBuffer);
            if (rc > 0) {
                QueuedQueries.push(TModbusQuery(queryBuffer, rc, modbus_get_header_length(_context), s));
                num_msgs++;
            } else {
                // TODO: error handling
                LOG(Debug) << "Modbus closed connection";
                close(s);
                FD_CLR(s, &refset);

                if (s == fd_max)
                    fd_max--;
            }
        }
    }

    return num_msgs;
}

bool TModbusTCPBackend::Available()
{
    return !QueuedQueries.empty();
}

TModbusQuery TModbusTCPBackend::ReceiveQuery(bool block)
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

void TModbusTCPBackend::Reply(const TModbusQuery &q)
{
    if (q.size <= 0) 
        return;

    uint8_t slave_id = 0;
    if (q.header_length > 0)
        slave_id = q.data[q.header_length - 1];

    modbus_set_socket(_context, q.socket_fd);

    if (_mappings.find(slave_id) == _mappings.end())
        throw ModbusException(std::string("Trying to reply on query with unknown slave ID ") 
                + std::to_string(slave_id));

    if (modbus_reply(_context, q.data, q.size, _mappings[slave_id]) < 0)
        _error = errno;
}

void TModbusTCPBackend::ReplyException(TReplyState e, const TModbusQuery &q)
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

void TModbusTCPBackend::Close()
{
    fd_max = -1;
}

int TModbusTCPBackend::GetError()
{
    return _error;
}
