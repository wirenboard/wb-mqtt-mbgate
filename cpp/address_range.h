#pragma once

#include <map>
#include <string>
#include <iostream>
#include <memory>
#include <utility>
#include <exception>


class WrongSegmentException : std::exception
{
    std::string msg;
public:
    WrongSegmentException(const std::string &_msg): msg(_msg) {}
    virtual const char *what() const throw()
    {
        return msg.c_str();
    }
};

/*!
 * Address range type template
 * Supports operating with address segments and store
 * additional data with each segment
 */
template<typename T> class TAddressRange
{
public:
    TAddressRange(int start, unsigned count, T obs)
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
    void insert(int start, unsigned count, T obs)
    {
        int end = start + count;

        /* check previous neighbor range to merge */
        auto prev = m.lower_bound(start);
        if (prev != m.begin())
            --prev;
        if (prev != m.end() && prev->second.first >= start && prev->first < start) {  // have intersection with previous segment
            if (prev->second.second != obs) {
                if (prev->second.first > start) {
                    throw WrongSegmentException("intersected observers don't match");
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
                throw WrongSegmentException("intersected observers don't match");
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
                prev--;
            else
                throw WrongSegmentException("incorrect segment");
        }

        if (prev != m.end() && prev->second.first >= int(start + count))
            return prev->second.second;

        throw WrongSegmentException("incorrect segment");
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

    void clear()
    {
        m.clear();
    }

    template<typename U>
    friend std::ostream& operator<<(std::ostream &str, const TAddressRange<U>& range);

protected:
    std::map<int, std::pair<int, T>> m;
};


template<typename T>
std::ostream& operator<<(std::ostream& str, const TAddressRange<T>& range)
{
        for (const auto &p : range.m) {
                str << "[" << p.first << ", " << p.second.first << ")" << std::endl;
        }

        return str;
}
