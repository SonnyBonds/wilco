#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "core/os.h"

template<typename ValueType>
struct ListPropertyValue
{
    // Constructors
    ListPropertyValue(bool allowDuplicates = false)
        : _allowDuplicates(allowDuplicates)
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    { }

    ListPropertyValue(const ListPropertyValue& other)
        : _allowDuplicates(other._allowDuplicates)
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    {
        *this += other;
    }

    ListPropertyValue(ListPropertyValue&& other)
        : _allowDuplicates(other._allowDuplicates)
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    {
        // This one could move the actual value container and tracker, but doing a simpler path right now
        *this += std::move(other);
    }

    // ListPropertyValue
    ListPropertyValue& operator=(const ListPropertyValue& other)
    {
        _value.clear();
        _duplicateTracker.clear();
        return *this += other._value;
    }

    ListPropertyValue& operator=(ListPropertyValue&& other)
    {
        _value.clear();
        _duplicateTracker.clear();
        return *this += std::move(other._value);
    }

    // ListPropertyValue<T>
    template<typename T>
    ListPropertyValue& operator=(const ListPropertyValue<T>& other)
    {
        _value.clear();
        _duplicateTracker.clear();
        return *this += other._value;
    }

    template<typename T>
    ListPropertyValue& operator=(ListPropertyValue<T>&& other)
    {
        _value.clear();
        _duplicateTracker.clear();
        return *this += std::move(other._value);
    }

    template<typename T>
    ListPropertyValue& operator+=(const ListPropertyValue<T>& other)
    {
        return *this += other._value;
    }

    template<typename T>
    ListPropertyValue& operator+=(ListPropertyValue<T>&& other)
    {
        return *this += std::move(other._value);
    }

    // generic element
    template<typename T>
    ListPropertyValue& operator =(T other)
    {
        _value.clear();
        *this += std::move(other);
        return *this;
    }
    
    template<typename T>
    ListPropertyValue& operator +=(T other) {
        // This whole thing is pretty sad, but we need to push the value into the list
        // first even if it's a duplicate, because the duplicate tracker can only compare 
        // items that are actually in the value list.
        _value.push_back(std::move(other));
        if(!_allowDuplicates)
        {
            auto it = _duplicateTracker.insert((int)_value.size() - 1);
            if (!it.second)
            {
                // If it already existed, we still update it because there are cases where items have the same main value/hash but still wants secondary values/options overwritten
                _value[*it.first] = std::move(_value.back());
                _value.pop_back();
            }
        }
        return *this;
    }

    // initializer list
    template<typename T>
    ListPropertyValue& operator =(std::initializer_list<T> other) {
        _value.clear();
        *this += std::move(other);
        return *this;
    }

    template<typename T>
    ListPropertyValue& operator +=(std::initializer_list<T> other) {
        _value.reserve(_value.size() + other.size());
        for(auto& e : other)
        {
            *this += std::move(e);
        }
        return *this;
    }

    // vector
    template<typename T>
    ListPropertyValue& operator =(std::vector<T> other) {
        _value.clear();
        *this += std::move(other);
        return *this;
    }

    template<typename T>
    ListPropertyValue& operator +=(std::vector<T> other) {
        _value.reserve(_value.size() + other.size());
        for(auto& e : other)
        {
            *this += std::move(e);
        }
        return *this;
    }

    auto begin() const
    {
        return _value.begin();
    }

    auto end() const
    {
        return _value.end();
    }

    size_t size() const
    {
        return _value.size();
    }

    bool empty() const
    {
        return _value.empty();
    }

    void clear()
    {
        _value.clear();
    }

    const std::vector<ValueType>& vector() const
    {
        return _value;
    }
    
private:
    struct IndexedValueEquals
    {
        IndexedValueEquals(const std::vector<ValueType>& values)
            : _values(values)
        { }

        bool operator()(int a, int b) const
        {
            return _values[a] == _values[b];
        }

    private:
        const std::vector<ValueType>& _values;
    };

    struct IndexedValueHash
    {
        IndexedValueHash(const std::vector<ValueType>& values)
            : _values(values)
        { }

        size_t operator()(int i) const
        {
            if constexpr (std::is_same_v<std::filesystem::path, ValueType>)
            {
                return std::filesystem::hash_value(_values[i]);
            }
            else
            {
                return std::hash<ValueType>{}(_values[i]);
            }
        }

    private:
        const std::vector<ValueType>& _values;
    };

    bool _allowDuplicates = false;
    std::vector<ValueType> _value;
    std::unordered_set<int, IndexedValueHash, IndexedValueEquals> _duplicateTracker;
};
