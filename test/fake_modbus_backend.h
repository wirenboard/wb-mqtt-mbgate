#pragma once

#include "modbus_wrapper.h"
#include <map>
#include <queue>

class TFakeModbusBackend: public IModbusBackend
{
public:
    virtual ~TFakeModbusBackend()
    {
        for (auto& slave: Caches) {
            for (auto& item: slave.second) {
                if (item.second) {
                    if (item.first == COIL || item.first == DISCRETE_INPUT)
                        delete[] static_cast<uint8_t*>(item.second);
                    else
                        delete[] static_cast<uint16_t*>(item.second);
                }
            }
        }
    }

    virtual void SetSlave(uint8_t slave_id)
    {
        _slaveId = slave_id;
    }

    virtual uint8_t GetSlave()
    {
        return _slaveId;
    }

    virtual void SetDebug(bool debug)
    {}

    virtual void Listen()
    {}

    /*! Allocate cache areas for this instance and given slave ID
     * \param slave_id Slave ID
     * \param di Number of discrete inputs
     * \param co Number of coils
     * \param ir Number of input registers (2 bytes per register, so ir * 2 bytes will be allocated)
     * \param hr Number of holding registers
     */
    virtual void AllocateCache(uint8_t slave_id, size_t di, size_t co, size_t ir, size_t hr)
    {
        if (!Caches[slave_id].empty())
            return; // no reallocation

        Caches[slave_id][DISCRETE_INPUT] = new uint8_t[di];
        Caches[slave_id][COIL] = new uint8_t[co];
        Caches[slave_id][INPUT_REGISTER] = new uint16_t[ir];
        Caches[slave_id][HOLDING_REGISTER] = new uint16_t[hr];
    }

    /*! Get cache base address
     * \param type Store type we want to get cache for
     * \return Pointer to cache base address
     */
    virtual void* GetCache(TStoreType type, uint8_t slave_id = 0)
    {
        if (Caches.find(slave_id) == Caches.end())
            throw TModbusException(std::string("Cache for slave id ") + std::to_string(slave_id) + " is not allocated");

        return Caches[slave_id][type];
    }

    virtual int WaitForMessages(int timeout = -1)
    {
        return IncomingQueries.size();
    }

    /*! Receive new query from instance
     * \return New query (or .size <= 0 on error)
     */
    virtual TModbusQuery ReceiveQuery(bool block = false)
    {
        if (!IncomingQueries.empty()) {
            auto ret = IncomingQueries.front();
            IncomingQueries.pop();
            return ret;
        }

        return TModbusQuery::emptyQuery();
    }

    virtual bool Available()
    {
        return !IncomingQueries.empty();
    }

    virtual void Close()
    {}

    /*! Send reply
     * \param query Query to reply on
     */
    virtual void Reply(const TModbusQuery& query)
    {
        RepliedQueries.push(query);
    }

    /*! Send exception reply
     * \param exception Exception code
     */
    virtual void ReplyException(TReplyState state, const TModbusQuery& query)
    {
        RepliedQueries.push(TModbusQuery::exceptionQuery(state));
    }

    /*! Get last error code */
    virtual int GetError()
    {
        return 0;
    }

    /*! Get last error message*/
    virtual const char* GetStrError()
    {
        return "";
    }

    /***********************************************
     * Fake backend methods
     */
    void PushQuery(const TModbusQuery& q)
    {
        IncomingQueries.push(q);
    }

public:
    std::map<uint8_t, std::map<TStoreType, void*>> Caches;
    std::queue<TModbusQuery> IncomingQueries;
    std::queue<TModbusQuery> RepliedQueries;

protected:
    uint8_t _slaveId;
};
