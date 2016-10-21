#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <exception>


class WrongSegmentException : std::exception
{
    std::string msg;
    int start;
    unsigned count;
public:
    WrongSegmentException(int _start, unsigned _count): start(_start), count(_count) {}
    WrongSegmentException(const std::string &_msg): msg(_msg) {}
    virtual const char *what() const throw()
    {
        if (msg.size() > 0) {
            return msg.c_str();
        } else {
            /* std::stringstream out; */
            /* out << "Overlapping segment: [" << start << ":" << start + count - 1 << "]"; */
            /* return out.str().c_str(); */
            return "Overlapping segments";
        }
    }

    virtual int GetStart() const throw()
    {
        return start;
    }

    virtual unsigned GetCount() const throw()
    {
        return count;
    }

    virtual ~WrongSegmentException() throw() {}
};

/*!
 * Address range type template
 * Supports operating with address segments and store
 * additional data with each segment
 */
template<typename T> class TAddressRange
{
public:

    TAddressRange(int start, unsigned count, T obs = T())
    {
        insert(start, count, obs);
    }

    // default empty constructor
    TAddressRange() {}

    /*! 
     * Insert new data segment
     * \param start First address in segment
     * \param count Number of units in segment
     * \param obs User param for this segment
     */
    void insert(int start, unsigned count, T obs = T())
    {
        int end = start + count;

        /* check previous neighbor range to merge */
        auto prev = m.lower_bound(start);
        if (prev != m.begin())
            --prev;
        if (prev != m.end() && prev->second.first >= start && prev->first < start) {  // have intersection with previous segment
            if (prev->second.second != obs) {
                if (prev->second.first > start) {
                    throw WrongSegmentException(start, count);
                }
            } else {
                start = prev->first;
            }

            /* m.erase(prev); */
        }
        
        /* merge all inner segments */
        auto inner = m.lower_bound(start);
        while (inner != m.end() && inner->first < end) {  
            auto& current_observer = inner->second.second;
            auto current_end = inner->second.first;
            // check if there are different observers
            if (current_observer != obs) {
                throw WrongSegmentException(start, count);
            }

            if (current_end > end)
                end = current_end;

            // prepare iterator to remove
            auto to_remove = inner;
            inner++;
            m.erase(to_remove);
        }

        /* create new large segment */
        m.insert(make_pair(start, std::make_pair(end, obs)));
    }

    /*!
     * Merge current range with external
     */
    void insert(const TAddressRange& range)
    {
        for (auto& segment : range.m)
            insert(segment.first, segment.second.first - segment.first, segment.second.second);
    }

    /*! Alias for insert(const TAddressRange&) */
    TAddressRange& operator+=(const TAddressRange& range)
    {
        insert(range);
        return *this;
    }

    TAddressRange operator+(const TAddressRange& range) const 
    {
        TAddressRange ret;
        ret.insert(*this);
        ret.insert(range);

        return ret;
    }

    void insert(const TAddressRange& range, T obs)
    {
        for (auto& segment : range.m)
            insert(segment.first, segment.second.first - segment.first, obs);
    }

    /*!
     * Check if segment is in this range
     * \param start First address in segment
     * \param count Number of units in segment
     * \return true is whole segment is in this range
     */
    bool inRange(int start, unsigned count = 1) const
    {
        try {
            getParam(start, count);
        } catch (const WrongSegmentException&) {
            return false;
        }

        return true;
    }

    /*!
     * Get address of first item in range
     */
    int getStart() const
    {
        auto b = m.cbegin();
        if (b == m.cend())
            throw WrongSegmentException("empty range");

        return b->first;
    }

    /*! 
     * Get upper bound of range (maximum address + 1)
     */
    int getEnd() const
    {
        auto b = m.cend();
        if (b == m.cbegin())
            throw WrongSegmentException("empty range");

        --b;
        return b->second.first;
    }

    /*!
     * Get number of items between start and end
     */
    int getCount() const
    {
        return getEnd() - getStart();
    }

    /*!
     * Get single param in case of single-segment range
     */
    T getParam() const
    {
        if (m.size() != 1)
            throw WrongSegmentException("range is not single-segment");

        return m.cbegin()->second.second;
    }

    /*!
     * Get user param for given segment
     * \param start First address in segment
     * \param count Number of units in segment
     * \return User parameter
     */
    T getParam(int start, unsigned count = 1) const
    {
        auto prev = m.lower_bound(start);
        if (prev->first != start) {
            if (prev != m.begin())
                --prev;
            else
                throw WrongSegmentException("incorrect segment");
        }

        if (prev != m.end() && prev->second.first >= int(start + count))
            return prev->second.second;

        throw WrongSegmentException("incorrect segment");
    }

    /*!
     * Get list of segments for bound segments.
     * If segments in given area aren't bound (follows one another)
     * exception will be thrown.
     * \param start First address in area
     * \param count Number of units in area
     * \return Vector of single-segment ranges
     */
    std::vector<TAddressRange<T>> getSegments(int start, int count = 1) const
    {
        std::vector<TAddressRange<T>> reply;

        auto prev = m.lower_bound(start);
        if (prev->first != start) {
            if (prev != m.begin())
                --prev;
            else
                throw WrongSegmentException("area out of bounds");
        }

        while (count > 0 && prev != m.end()) {
            const int s_size = prev->second.first - start;

            reply.push_back(TAddressRange<T>(start, std::min(s_size, count), prev->second.second));

            start += s_size;
            count -= s_size;
            ++prev;

            if (count > 0 && prev->first != start)
                throw WrongSegmentException("sparce area");
        }

        if (count > 0)
            throw WrongSegmentException("area out of bounds");

        return reply;
    }

    bool operator==(const TAddressRange<T>& r) const
    {
        for (auto &segment : m) {
            auto v = r.m.find(segment.first);
            if (v == r.m.end() || v->second != segment.second)
                return false;
        }

        return true;
    }

    /*! Shift range by given offset
     * \param offset Offset
     */
    void shift(int offset)
    {
        std::map<int, std::pair<int, T>> nmap;

        for (auto &segment : m) {
            segment.second.first += offset;
            nmap[segment.first + offset] = segment.second;
        }

        m = nmap;
    }

    /*! Alias for shift()
     */
    TAddressRange operator+(int offset) const
    {
        TAddressRange ret;
        ret.insert(*this);
        ret.shift(offset);
        return ret;
    }

    TAddressRange operator-(int offset) const
    {
        return operator+(-offset);
    }

    TAddressRange& operator+=(int offset)
    {
        shift(offset);
        return *this;
    }

    TAddressRange& operator-=(int offset)
    {
        return operator+=(-offset);
    }


    void clear()
    {
        m.clear();
    }

    /*! Iterator */
    typename std::map<int, std::pair<int, T>>::const_iterator cbegin() const { return m.cbegin(); }
    typename std::map<int, std::pair<int, T>>::const_iterator cend() const { return m.cend(); }
    
    typedef typename std::map<int, std::pair<int, T>>::const_iterator const_iterator;

    template<typename U>
    friend std::ostream& operator<<(std::ostream &str, const TAddressRange<U>& range);

protected:
    std::map<int, std::pair<int, T>> m;
};


template<typename T>
std::ostream& operator<<(std::ostream& str, const TAddressRange<T>& range)
{
        for (const auto &p : range.m) {
                str << "[" << p.first << ", " << p.second.first << ") => " << p.second.second << std::endl;
        }

        return str;
}
