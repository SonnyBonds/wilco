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


namespace detail
{

template<typename ValueType>
struct ListPropertyValue
{
    // Constructors
    ListPropertyValue(bool allowDuplicates)
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
    std::vector<ValueType> _value;
    std::unordered_set<int, IndexedValueHash, IndexedValueEquals> _duplicateTracker;
};

}

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
        if(group)
        {
            group->bag->properties.push_back(this);
        }
    }

    virtual void import(const PropertyBase& other) = 0;
};

template<typename ValueType>
struct Property : public PropertyBase
{
    struct ConfigValue
    {
        ConfigValue(StringId config)
            : _config(config)
            , _value{}
        { }

        ConfigValue(StringId config, ValueType value)
            : _config(config)
            , _value{std::move(value)}
        { }

        ConfigValue(const ConfigValue&) = default;
        ConfigValue(ConfigValue&&) = default;

        ConfigValue& operator =(const ConfigValue& other)
        {
            _set = other._set;
            _value = other._value;
            return *this;
        }

        ConfigValue& operator =(ConfigValue&& other)
        {
            _set = other._set;
            _value = std::move(other._value);
            return *this;
        }

        template<typename T>
        ConfigValue& operator =(T&& value)
        {
            _set = true;
            _value = std::forward<T>(value);
            return *this;
        }

        template<typename T>
        ConfigValue& operator +=(T&& value)
        {
            _set = true;
            _value += std::forward<T>(value);
            return *this;
        }

        operator const ValueType&() const
        {
            return _value;
        }

        const ValueType& value() const
        {
            return _value;
        }

        bool isSet() const
        {
            return _set;
        }
    private:
        bool _set = false;
        ValueType _value;
        StringId _config;

        friend class Property;
        template<typename T> friend struct ListProperty;
    };

    Property(PropertyGroup* group)
        : PropertyBase(group)
        , _baseValue({})
    { }

    Property()
        : Property(nullptr)
    { }

    // Property<T>
    template<typename T>
    Property& operator =(const Property<T>& other)
    {
        _baseValue = other._baseValue;
        _confValues.clear();
        for(auto& confValue : other._confValues)
        {
            if(confValue.isSet())
            {
                _confValues.emplace_back(confValue._config, confValue._value);
            }
            else
            {
                _confValues.emplace_back(confValue._config);
            }
        }
        return *this;
    }

    template<typename T>
    Property& operator +=(const Property<T>& other)
    {
        for(auto& confValue : _confValues)
        {
            if(confValue.isSet())
            {
                if(auto otherConfValue = other.getConfigValueIfSet(confValue._config))
                {
                    confValue._value += otherConfValue->_value;
                }
                else if(other._baseValue.isSet())
                {
                    confValue._value += other._baseValue;
                }
            }
        }
        for(auto& otherConfValue : other._confValues)
        {
            if(otherConfValue.isSet())
            {
                auto confValue = getConfigValueIfSet(otherConfValue._config);
                if(!confValue)
                {
                    getConfigValue(otherConfValue._config)._value += otherConfValue._value;
                }
            }
        }
        if(other._baseValue.isSet())
        {
            _baseValue += other._baseValue;
        }
        return *this;
    }

    // generic value
    template<typename T>
    Property& operator =(T value)
    {
        _baseValue = std::move(value);
        _confValues.clear();
        return *this;
    }

    template<typename T>
    Property& operator +=(T value)
    {
        for(auto& confValue : _confValues)
        {
            confValue._value += value;
        }
        _baseValue += std::move(value);
        return *this;
    }

    ConfigValue& operator ()(StringId config)
    {
        return getConfigValue(config);
    }

    const ConfigValue& operator ()(StringId config) const
    {
        return getConfigValue(config);
    }

private:
    ConfigValue* getConfigValueIfSet(StringId config) const
    {
        if(auto confValue = getConfigValueIfAvailable(config))
        {
            if(confValue->isSet())
            {
                return confValue;
            }
        }
        return nullptr;
    }
    
    ConfigValue* getConfigValueIfAvailable(StringId config) const
    {
        for(auto& value : _confValues)
        {
            if(value._config == config)
            {
                return &value;
            }
        }

        return nullptr;
    }

    ConfigValue& getConfigValue(StringId config) const
    {
        if(auto existing = getConfigValueIfAvailable(config))
        {
            if(!existing->isSet() && _baseValue.isSet())
            {
                *existing = _baseValue;
            }
            return *existing;
        }

        _confValues.emplace_back(config);
        _confValues.back() = _baseValue;
        return _confValues.back();
    }

    void import(const PropertyBase& other)
    {
        auto& otherProperty = static_cast<const Property&>(other);
        bool otherSet = otherProperty._baseValue.isSet();
        if(!otherSet)
        {
            for(auto& confValue : otherProperty._confValues)
            {
                if(confValue.isSet())
                {
                    otherSet = true;
                    break;
                }
            }
        }

        if(!otherSet)
        {
            return;
        }

        *this = otherProperty;
    }

    ConfigValue _baseValue;
    mutable std::vector<ConfigValue> _confValues;

    template<typename T> friend struct ListProperty;
};

template<typename ValueType>
struct ListProperty : public PropertyBase
{
    struct ConfigValue
    {
        ConfigValue(StringId config, bool allowDuplicates)
            : _config(config)
            , _value(allowDuplicates)
        { }

        ConfigValue(const ConfigValue&) = delete;
        ConfigValue(ConfigValue&&) = default;

        template<typename T>
        ConfigValue& operator =(T&& value)
        {
            _value = std::forward<T>(value);
            return *this;
        }

        template<typename T>
        ConfigValue& operator +=(T&& value)
        {
            _value += std::forward<T>(value);
            return *this;
        }

        template<typename T>
        ConfigValue& operator =(std::initializer_list<T> value)
        {
            _value = std::move(value);
            return *this;
        }

        template<typename T>
        ConfigValue& operator +=(std::initializer_list<T> value)
        {
            _value += std::move(value);
            return *this;
        }

        operator const detail::ListPropertyValue<ValueType>&() const
        {
            return _value;
        }

        const detail::ListPropertyValue<ValueType>& value() const
        {
            return _value;
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
    private:
        detail::ListPropertyValue<ValueType> _value;
        StringId _config;

        friend class ListProperty;
    };
    
    ListProperty(bool allowDuplicates = false)
        : ListProperty(nullptr, allowDuplicates)
    { }
    
    ListProperty(PropertyGroup* group, bool allowDuplicates = false)
        : PropertyBase(group)
        , _allowDuplicates(allowDuplicates)
        , _baseValue(allowDuplicates)
    { }

    ListProperty(const ListProperty& other) = delete;

    // ListProperty
    ListProperty& operator =(const ListProperty& property)
    {
        _confValues.clear();
        *this += property;
        return *this;
    }

    // ListProperty<T>
    template<typename T>
    ListProperty& operator =(const ListProperty<T>& property)
    {
        _confValues.clear();
        *this += property;
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(const ListProperty<T>& other)
    {
        for(auto& confValue : _confValues)
        {
            if(auto otherConfValue = other.getConfigValueIfAvailable(confValue._config))
            {
                confValue._value += otherConfValue->_value;
            }
            else
            {
                confValue._value += other._baseValue;
            }
        }
        for(auto& otherConfValue : other._confValues)
        {
            auto confValue = getConfigValueIfAvailable(otherConfValue._config);
            if(!confValue)
            {
                getConfigValue(otherConfValue._config)._value += otherConfValue._value;
            }
        }
        _baseValue += other._baseValue;
        return *this;
    }

    // Property<T>
    template<typename T>
    ListProperty& operator =(const Property<T>& property)
    {
        _confValues.clear();
        *this += property;
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(const Property<T>& other)
    {
        for(auto& confValue : _confValues)
        {
            if(auto otherConfValue = other.getConfigValueIfSet(confValue._config))
            {
                confValue._value += otherConfValue->_value;
            }
            else if(other._baseValue.isSet())
            {
                confValue._value += other._baseValue;
            }
        }
        for(auto& otherConfValue : other._confValues)
        {
            if(otherConfValue.isSet())
            {
                auto confValue = getConfigValueIfAvailable(otherConfValue._config);
                if(!confValue)
                {
                    getConfigValue(otherConfValue._config)._value += otherConfValue._value;
                }
            }
        }
        if(other._baseValue.isSet())
        {
            _baseValue += other._baseValue;
        }
        return *this;
    }
    
    // generic value
    template<typename T>
    ListProperty& operator =(T value)
    {
        _baseValue = std::move(value);
        _confValues.clear();
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(T value)
    {
        for(auto& confValue : _confValues)
        {
            confValue._value += value;
        }
        _baseValue += std::move(value);
        return *this;
    }

    // initializer_list
    template<typename T>
    ListProperty& operator =(std::initializer_list<T> value)
    {
        _confValues.clear();
        _baseValue.clear();
        *this += std::move(value);
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(std::initializer_list<T> value)
    {
        for(auto& confValue : _confValues)
        {
            confValue._value += value;
        }
        _baseValue += value;
        return *this;
    }

    // vector
    template<typename T>
    ListProperty& operator =(const std::vector<T>& value)
    {
        _confValues.clear();
        _baseValue.clear();
        *this += std::move(value);
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(std::vector<T> value)
    {
        for(auto& confValue : _confValues)
        {
            confValue._value += value;
        }
        _baseValue += value;
        return *this;
    }

    ConfigValue& operator ()(StringId config)
    {
        return getConfigValue(config);
    }

    const ConfigValue& operator ()(StringId config) const
    {
        return getConfigValue(config);
    }

private:
    ConfigValue* getConfigValueIfAvailable(StringId config) const
    {
        for(auto& value : _confValues)
        {
            if(value._config == config)
            {
                return &value;
            }
        }

        return nullptr;
    }

    ConfigValue& getConfigValue(StringId config) const
    {
        if(auto existing = getConfigValueIfAvailable(config))
        {
            return *existing;
        }

        _confValues.emplace_back(config, _allowDuplicates);
        _confValues.back()._value = _baseValue;
        return _confValues.back();
    }

    void import(const PropertyBase& other)
    {
        auto& otherListProperty = static_cast<const ListProperty&>(other);
        *this += otherListProperty;
    }

    detail::ListPropertyValue<ValueType> _baseValue;
    mutable std::vector<ConfigValue> _confValues;
    bool _allowDuplicates;
};

template<typename KeyType, typename ValueType>
using MapProperty = Property<std::map<KeyType, ValueType>>;
