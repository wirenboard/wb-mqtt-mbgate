#pragma once

/*!\file    modbus_wrapper.h
 * \brief   Lightweight Modbus interface
 * \author  Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#include <memory>
#include <list>
#include <cstdint>
#include <cstring>

#include "address_range.h"

/*! Modbus store types */
enum TStoreType {
    DISCRETE_INPUT = 1,
    COIL = 2,
    INPUT_REGISTER = 4,
    HOLDING_REGISTER = 8
};

inline const std::string StoreTypeToString(TStoreType t)
{
    if (t == DISCRETE_INPUT) {
        return "discrete inputs";
    } else if (t == COIL) {
        return "coils";
    } else if (t == INPUT_REGISTER) {
        return "input registers";
    } else if (t == HOLDING_REGISTER) {
        return "holding registers";
    } else {
        return "[unknown store type]";
    }
}

enum TReplyState {
    REPLY_CACHED            = -1,  /*!< Don't use observer, use cached value instead */
    REPLY_OK                = 0,   /*!< Reply is correct */

    REPLY_ILLEGAL_FUNCTION  = 0x01,
    REPLY_ILLEGAL_ADDRESS   = 0x02, /*!< Wrong address for this datablock */
    REPLY_ILLEGAL_VALUE     = 0x03, /*!< Wrong value given for this datablock */
    REPLY_SERVER_FAILURE    = 0x04, /*!< Server failure */
};


typedef TAddressRange<void *> TModbusCacheAddressRange;

/*!
 * \brief Modbus server observer interface
 * Modbus server observer is able to reply on Modbus' READ_*, WRITE_* for coils and registers.
 */
class IModbusServerObserver
{
public:
    /* Virtual destructor */
    virtual ~IModbusServerObserver();

    /*! Callback for READ_* functions
     * Called before replying to Modbus client
     * May be ignored if observer stores cached value in server context
     * \param type      Type of store
     * \param unit_id   Modbus TCP unit ID (or ID of serial device in case of RTU)
     * \param start     First element address
     * \param count     Number of elements required
     * \param data      Pointer to request data (already allocated)
     * \return Reply state
     */
    virtual TReplyState OnGetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void *data);

    /*! Callback for WRITE_* functions
     * Called before write action, may refuse writing if don't return REPLY_OK
     * May be ignored if observer stores cached value in server context
     * \param type      Type of store
     * \param unit_id   Modbus TCP unit ID (or ID of serial device in case of RTU)
     * \param start     First element address
     * \param count     Number of elements required
     * \param data      Pointer to query data
     * \return Reply state
     */
    virtual TReplyState OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data);

    /*! Cache allocation callback
     * Modbus server tells about allocated cache memory
     * \param type      Type of store
     * \param cache     Cache address range with area begin pointers
     * \param unit_id   Unit (slave) ID
     */
    virtual void OnCacheAllocate(TStoreType type, uint8_t unit_id, const TModbusCacheAddressRange& cache);
};

/*! Shared pointer to IModbusServerObserver */
typedef std::shared_ptr<IModbusServerObserver> PModbusServerObserver;

/*! Definition of Modbus address range */
typedef TAddressRange<PModbusServerObserver> TModbusAddressRange;
typedef std::shared_ptr<TModbusAddressRange> PModbusAddressRange;

/*! Modbus query structure */
struct TModbusQuery
{
    static TModbusQuery emptyQuery()
    {
        return TModbusQuery(nullptr, 0, 0, -1);
    }

    static TModbusQuery exceptionQuery(TReplyState state)
    {
        if (state > 0)
            return TModbusQuery(nullptr, -state, 0, -1);

        return TModbusQuery::emptyQuery();
    }

    uint8_t *data; /*!< Query raw data */
    int size; /*!< Query size */
    int header_length; /*!< Query header length - from Modbus context */
    int socket_fd; /*!< Reply socket descriptor (for TCP) */

    TModbusQuery(uint8_t *_data, int _size, int _header_length, int _fd = -1)
        : data(nullptr)
        , size(_size)
        , header_length(_header_length)
        , socket_fd(_fd)
    {
        if (size > 0) {
            data = new uint8_t[size];
            std::memcpy(data, _data, size);
        }
    }

    TModbusQuery(const TModbusQuery &q)
        : data(nullptr)
        , size(q.size)
        , header_length(q.header_length)
        , socket_fd(q.socket_fd)
    {
        if (data)
            delete [] data;

        if (size > 0) {
            data = new uint8_t[size];
            std::memcpy(data, q.data, size);
        }
    }

    ~TModbusQuery()
    {
        if (size > 0)
            delete [] data;
    }
};

/*! Modbus exception */
class TModbusException : public std::exception
{
    std::string msg;
public:
    TModbusException(const std::string& _msg) : msg(_msg) {}
    virtual const char *what() const throw()
    {
        return msg.c_str();
    }
    virtual ~TModbusException() throw() {}
};

/*! Modbus backend interface
 * This is a way to operate with Modbus library (or mock in testing)
 */
class IModbusBackend : public std::enable_shared_from_this<IModbusBackend>
{
public:
    /*! Set slave ID for this instance
     * \param slave_id Slave ID (from 1 to 254, 255 == 0xFF - for broadcast receives)
     */
    virtual void SetSlave(uint8_t slave_id = 0xFF) = 0;

    /*! Get current slave ID */
    virtual uint8_t GetSlave() = 0;

    /*! Start listening port/socket */
    virtual void Listen() = 0;

    /*! Allocate cache areas for this instance
     * \param unit_id   Unit ID to allocate cache for
     * \param di Number of discrete inputs
     * \param co Number of coils
     * \param ir Number of input registers (2 bytes per register, so ir * 2 bytes will be allocated)
     * \param hr Number of holding registers
     */
    virtual void AllocateCache(uint8_t unit_id, size_t di, size_t co, size_t ir, size_t hr) = 0;

    /*! Get cache base address
     * \param type Store type we want to get cache for
     * \return Pointer to cache base address
     */
    virtual void *GetCache(TStoreType type, uint8_t slave_id = 0) = 0;

    /*! Poll new queries and fill queue
     * \param timeout Poll timeout (in ms)
     * \return Number of messages received or -1 on error
     */
    virtual int WaitForMessages(int timeout = -1) = 0;

    /*! Check if messages are available
     * \return true if there are messages ready to read
     */
    virtual bool Available() = 0;

    /*! Receive query from queue (or wait for new query)
     * \param block Blocking call (wait for new query on empty queue)
     * \return New query, or .size == 0 on empty queue
     */
    virtual TModbusQuery ReceiveQuery(bool block = false) = 0;

    /*! Send reply
     * \param query Query to reply on
     */
    virtual void Reply(const TModbusQuery &query) = 0;

    /*! Send exception reply
     * \param exception Exception code
     */
    virtual void ReplyException(TReplyState state, const TModbusQuery &query) = 0;

    /*! Get last error code */
    virtual int GetError() = 0;

    /*! Close connection */
    virtual void Close() = 0;

    /*! Virtual destructor */
    virtual ~IModbusBackend();
};

/*! Shared pointer to IModbusBackend */
typedef std::shared_ptr<IModbusBackend> PModbusBackend;

/*! Modbus server wrapper base class */
class TModbusServer
{
    enum Command {
        READ_COIL_STATUS            = 0x01,
        READ_DISCRETE_INPUTS        = 0x02,
        READ_HOLDING_REGISTERS      = 0x03,
        READ_INPUT_REGISTERS        = 0x04,

        FORCE_SINGLE_COIL           = 0x05,
        PRESET_SINGLE_REGISTER      = 0x06,

        FORCE_MULTIPLE_COILS        = 0x0F,
        PRESET_MULTIPLE_REGISTERS   = 0x10
    };

    inline bool _IsReadCmd(Command cmd)
    {
        return (cmd >= READ_COIL_STATUS) && (cmd <= READ_INPUT_REGISTERS);
    }

    inline bool _IsSingleWriteCmd(Command cmd)
    {
        return (cmd == FORCE_SINGLE_COIL) || (cmd == PRESET_SINGLE_REGISTER);
    }

    inline bool _IsMultiWriteCmd(Command cmd)
    {
        return (cmd == FORCE_MULTIPLE_COILS) || (cmd == PRESET_MULTIPLE_REGISTERS);
    }

    inline bool _IsWriteCmd(Command cmd)
    {
        return _IsSingleWriteCmd(cmd) || _IsMultiWriteCmd(cmd);
    }

    inline bool _IsCoilWriteCmd(Command cmd)
    {
        return (cmd == FORCE_SINGLE_COIL) || (cmd == FORCE_MULTIPLE_COILS);
    }

    inline uint16_t _ReadU16(const uint8_t *data) const
    {
        return (*data << 8) | (*(data + 1));
    }

public:
    /*! Modbus server constructor */
    TModbusServer(PModbusBackend backend = PModbusBackend());

    /*! Virtual destructor */
    virtual ~TModbusServer() {}

    /*! Set backend */
    void Backend(PModbusBackend backend);

    /*! Get backend */
    PModbusBackend Backend();

    /*! Modbus main loop function
     * \param timeoutMilliS wait timeout in milliseconds, -1 - block indefinitely
     * \return 0 on success, -1 on error
     */
    virtual int Loop(int timeoutMilliS = -1);

    /*! Cache allocation request
     * (Re)allocate cache values and tell observers about it
     */
    virtual void AllocateCache();

    /*! Register observer for given slave ID, store type and range
     * \param o Pointer to observer object
     * \param slave Slave ID
     * \param store Store type (member of TStoreType)
     * \param range Address range to handle
     */
    virtual void Observe(PModbusServerObserver o, TStoreType store, const TModbusAddressRange &range, uint8_t slave_id=0);
    /* virtual void Unobserve(PModbusServerObserver o) = 0; */

private:
    void _ProcessQuery(const TModbusQuery &query);
    void _ProcessReadQuery(TStoreType type, TModbusAddressRange &range, uint8_t slave_id, int start, unsigned count, const TModbusQuery &query);
    void _ProcessWriteQuery(TStoreType type, TModbusAddressRange &range, uint8_t slave_id, int start, unsigned count, const TModbusQuery &query, const void *data);

    std::map<Command, TModbusAddressRange *> _CmdRangeMap;
    std::map<Command, TStoreType> _CmdStoreTypeMap;

    struct TRSet {
        int di = 0;
        int co = 0;
        int ir = 0;
        int hr = 0;
    };

    std::map<uint8_t, TRSet> _maxSlaveAddresses;

protected:
    /*! Modbus address ranges */
    TModbusAddressRange _di, _co, _ir, _hr;


    PModbusBackend mb;
};

/*! Shared pointer to TModbusServer */
typedef std::shared_ptr<TModbusServer> PModbusServer;
