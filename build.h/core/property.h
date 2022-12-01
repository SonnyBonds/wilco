#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <map>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "core/os.h"
#include "core/stringid.h"

struct PropertyBase;
struct PropertyBag;

struct PropertyGroup
{
    PropertyGroup(PropertyBag* parent = nullptr)
        : bag(parent)
    { }

    PropertyGroup(PropertyGroup* group = nullptr)
        : bag(group->bag)
    { }

protected:
    PropertyBag* bag = nullptr;

    friend struct PropertyBase;
};

struct PropertyBag : public PropertyGroup
{
    PropertyBag()
        : PropertyGroup(this)
    { }

    std::vector<PropertyBase*> properties;
protected:

    friend struct Project;
    friend struct PropertyBase;
};

struct PropertyBase
{
    PropertyBase(PropertyGroup* group)
    {
        group->bag->properties.push_back(this);
    }
};

template<typename ValueType>
struct Property : public PropertyBase
{
    Property(PropertyGroup* group)
        : PropertyBase(group)
    { }

    /*template<typename U>
    Property& operator =(U&& v)
    {
        _value[""] = std::forward<U>(v);
        return *this;
    }

    template<typename U>
    Property& operator +=(U&& v)
    {
        _value[""] += std::forward<U>(v);
        return *this;
    }

    operator const ValueType&() const
    {
        return _value[""];
    }*/

    template<typename T>
    ValueType& operator =(T&& value)
    {
        return (*this)("") = std::forward<T>(value);
    }

    template<typename T>
    ValueType& operator +=(T&& value)
    {
        return (*this)("") += std::forward<T>(value);
    }

    ValueType& operator ()(StringId config) const
    {
        for(auto& value : _confValues)
        {
            if(value.first == config)
            {
                return value.second;
            }
        }
        _confValues.push_back(std::make_pair(config, ValueType{}));
        return _confValues.back().second;
    }

private:
    mutable std::vector<std::pair<StringId, ValueType>> _confValues;
};

namespace detail
{

template<typename ValueType>
struct ListPropertyValue
{
    ListPropertyValue(bool allowDuplicates)
        : _allowDuplicates(allowDuplicates)
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    { }

    ListPropertyValue(ListPropertyValue&& other)
        : _allowDuplicates(other._allowDuplicates)
        , _value(std::move(other._value))
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    { }

    ListPropertyValue(const ListPropertyValue& other) = delete;
    ListPropertyValue& operator=(const ListPropertyValue& other) = delete;
    ListPropertyValue& operator=(ListPropertyValue&& other) = delete;

    template<typename T>
    ListPropertyValue& operator =(T other)
    {
        if(_allowDuplicates)
        {
            _value = std::move(other);
        }
        else
        {
            _value.clear();
            _value.reserve(other.size());
            for(auto& e : other)
            {
                *this += std::move(e);
            }
        }
        return *this;
    }
    
    template<typename T>
    ListPropertyValue& operator +=(T other) {
        _value.push_back(std::move(other));
        if(!_allowDuplicates)
        {
            auto it = _duplicateTracker.insert(_value.size() - 1);
            if (!it.second)
            {
                // If it already existed, we still update it because there are cases where items have the same main value/hash but still wants secondary values/options overwritten
                _value[*it.first] = std::move(_value.back());
                _value.pop_back();
            }
        }
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
            if constexpr (std::is_base_of_v<StringId, ValueType>)
            {
                return std::hash<StringId>{}(_values[i]);                
            }
            else if constexpr (std::is_same_v<std::filesystem::path, ValueType>)
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
    std::vector<ValueType> _value{};
    std::unordered_set<int, IndexedValueHash, IndexedValueEquals> _duplicateTracker;
};

}

#if 0
template<typename ValueType>
struct ListProperty : public PropertyBase
{
    ListProperty(PropertyGroup* group, bool allowDuplicates = false)
        : PropertyBase(group)
        , _allowDuplicates(allowDuplicates)
    { }

    template<typename U>
    ListProperty& operator =(U&& v)
    {
        _value[""] = std::forward<U>(v);
        return *this;
    }

    template<typename U>
    ListProperty& operator +=(U&& v)
    {
        _value[""] += std::forward<U>(v);
        return *this;
    }

    operator const ValueType&() const
    {
        return _value[""];
    }

    ValueType& operator ()(StringId config = {})
    {
        return _value[config];
    }

    auto begin() const
    {
        return _value.begin();
    }

    auto end() const
    {
        return _value.end();
    }

private:
    detail::List<ValueType>& getOrCreate(StringId config)
    {
        auto it = _value.find(config);
        if(it != _value.end())
        {
            return it->second;
        }

        auto newEntry = _value.insert(std::make_pair(config, detail::List<ValueType>(_allowDuplicates)));
        return newEntry.first.second;
    }

    detail::List<ValueType>& getForWriting(StringId config)
    {
        return getOrCreate(config);
    }

    detail::List<ValueType>& getForReading(StringId config)
    {
        return getOrCreate(config);
    }

    mutable std::unordered_map<StringId, detail::List<ValueType>> _value;
    bool _allowDuplicates;
};
#endif

template<typename ValueType>
struct ListProperty : public PropertyBase
{
    ListProperty(PropertyGroup* group, bool allowDuplicates = false)
        : PropertyBase(group)
        , _allowDuplicates(allowDuplicates)
    { }

    template<typename T>
    detail::ListPropertyValue<ValueType>& operator =(T&& value)
    {
        return (*this)("") = std::forward<T>(value);
    }

    template<typename T>
    detail::ListPropertyValue<ValueType>& operator =(std::initializer_list<T>&& value)
    {
        return (*this)("") = std::forward<std::initializer_list<T>>(value);
    }

    template<typename T>
    detail::ListPropertyValue<ValueType>& operator +=(T&& value)
    {
        return (*this)("") += std::forward<T>(value);
    }

    template<typename T>
    detail::ListPropertyValue<ValueType>& operator +=(std::initializer_list<T>&& value)
    {
        return (*this)("") += std::forward<std::initializer_list<T>>(value);
    }

    detail::ListPropertyValue<ValueType>& operator ()(StringId config) const
    {
        for(auto& value : _confValues)
        {
            if(value.first == config)
            {
                return value.second;
            }
        }
        _confValues.emplace_back(config, detail::ListPropertyValue<ValueType>(_allowDuplicates));
        return _confValues.back().second;
    }

private:
    mutable std::vector<std::pair<StringId, detail::ListPropertyValue<ValueType>>> _confValues;
    bool _allowDuplicates;
};

template<typename KeyType, typename ValueType>
using MapProperty = Property<std::map<KeyType, ValueType>>;

#if 0
struct MapProperty : public PropertyBase
{
    MapProperty(PropertyGroup* group)
        : PropertyBase(group)
    { }

    MapProperty(const MapProperty& other) = delete;
    MapProperty(MapProperty&& other) = delete;
    MapProperty& operator=(const MapProperty& other) = delete;
    MapProperty& operator=(MapProperty&& other) = delete;

    template<typename T>
    MapProperty& operator =(T other)
    {
        _value.clear();
        _value.reserve(other.size());
        for(auto& e : other)
        {
            (*this)[e.first] += std::move(e.second);
        }
        return *this;
    }

    MapProperty& operator +=(std::pair<KeyType, ValueType> other) {
        _value[std::move(other.first)] = std::move(other.second);
        return *this;
    }

    MapProperty& operator +=(std::initializer_list<std::pair<KeyType, ValueType>> other) {
        _value.reserve(_value.size() + other.size());
        for(auto& e : other)
        {
            _value[std::move(e.first)] = std::move(e.second);
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

    ValueType& operator[](const KeyType& key)
    {
        return _value[key];
    }

    ValueType& operator[](KeyType&& key)
    {
        return _value[std::move(key)];
    }

    operator const std::unordered_map<KeyType, ValueType>&() const
    {
        return _value;
    }

    const std::unordered_map<KeyType, ValueType>& value() const
    {
        return _value;
    }

private:

    void applyOverlay(const PropertyBase& other)
    {
        auto& otherMapProperty = static_cast<const MapProperty&>(other);
        _value.reserve(_value.size() + otherMapProperty.value().size());
        for(auto& e : otherMapProperty)
        {
            _value[e.first] = e.second;
        }
    }

    std::unordered_map<KeyType, ValueType> _value{};
};

#endif