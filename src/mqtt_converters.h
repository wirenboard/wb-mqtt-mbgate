/*! \file mqtt_converters.h
 *  \brief Tools to convert messages from MQTT to Modbus registers and vice versa
 *  \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <memory>
#include <string>

#include <iostream>

/*! MQTT data type converter interface */
class IMQTTConverter: public std::enable_shared_from_this<IMQTTConverter>
{
public:
    /*! Unpack Modbus register value into MQTT-readable string
     * Must be thread safe!
     * \param data Pointer to start of Modbus record
     * \param size Modbus record size (in bytes)
     * \return MQTT string value
     */
    virtual std::string Unpack(const void* data, size_t size) const = 0;

    /*! Pack MQTT message into Modbus record
     * \param value MQTT message
     * \param data Pointer to Modbus record buffer
     * \param size Size of Modbus buffer
     * \return Pointer to Modbus buffer
     */
    virtual void* Pack(const std::string& value, void* data, size_t size) = 0;
};

/*! Shared pointer to IMQTTConverter */
typedef std::shared_ptr<IMQTTConverter> PMQTTConverter;

/***************************************************************************
 * Concrete data converters
 */

/*! Discrete type data converter */
class TMQTTDiscrConverter: public IMQTTConverter
{
public:
    std::string Unpack(const void* data, size_t size) const;
    void* Pack(const std::string& value, void* data, size_t size);
};

/*! Integer types data converter */
class TMQTTIntConverter: public IMQTTConverter
{
public:
    /*! Integer data representation */
    enum IntegerType
    {
        SIGNED = 1, /*!< Generic 2s-complement signed integer */
        UNSIGNED,   /*!< Generic unsigned integer */
        BCD         /*!< Binary-coded decimal */
    };

protected:
    IntegerType Type;

    /*! Swap bytes in each register */
    bool ByteSwap;

    /*! Swap registers (reverse order) */
    bool WordSwap;

    /*! Data scale multiplier (e.g. for "fixed point") */
    double Scale;

    /*! Size of resulting integer in bytes
     * Values will be rounded up to nearest power of 2 (<= 8)
     */
    unsigned Size;

public:
    TMQTTIntConverter(IntegerType type = SIGNED,
                      double scale = 1.0,
                      unsigned size = 2,
                      bool byteswap = false,
                      bool wordswap = false)
        : Type(type),
          ByteSwap(byteswap),
          WordSwap(wordswap),
          Scale(scale)
    {
        if (size > 4)
            Size = 8;
        else if (size > 2)
            Size = 4;
        else
            Size = 2;
    }

    std::string Unpack(const void* data, size_t size) const;
    void* Pack(const std::string& value, void* data, size_t size);
};

/*! IEEE 754 floating point converters */
class TMQTTFloatConverter: public IMQTTConverter
{
protected:
    /*! Value size in bytes, may be 4 or 8 for either "float" or "double" */
    unsigned Size;

    /*! Swap bytes in registers */
    bool ByteSwap;

    /*! Swap words (reverse order) */
    bool WordSwap;

    /*! Data scale multiplier (e.g. for "fixed point") */
    double Scale;

public:
    TMQTTFloatConverter(unsigned size = 4, bool byteswap = false, bool wordswap = false, double scale = 1.0)
        : ByteSwap(byteswap),
          WordSwap(wordswap),
          Scale(scale)
    {
        if (size > 4)
            Size = 8;
        else
            Size = 4;
    }

    std::string Unpack(const void* data, size_t size) const;
    void* Pack(const std::string& value, void* data, size_t size);
};

/*! Plain text data converters */
class TMQTTTextConverter: public IMQTTConverter
{
protected:
    /*! String length in bytes (will be presented as 1 symbol per register) */
    unsigned Size;

    /*! Swap bytes in registers */
    bool ByteSwap;

    /*! Swap words (reverse order) */
    bool WordSwap;

public:
    TMQTTTextConverter(unsigned size = 0, bool byteswap = false, bool wordswap = false)
        : Size(size),
          ByteSwap(byteswap),
          WordSwap(wordswap)
    {}

    std::string Unpack(const void* data, size_t size) const;
    void* Pack(const std::string& value, void* data, size_t size);
};
