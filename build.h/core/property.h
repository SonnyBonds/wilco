#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <map>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "core/os.h"
#include "core/stringid.h"

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

enum Transitivity
{
    Local,
    Public,
    PublicOnly
};

struct ConfigSelector
{
    ConfigSelector() {}
    ConfigSelector(StringId name)
        : name(name)
    {}

    ConfigSelector(const char* name)
        : name(name)
    {}

    ConfigSelector(std::string name)
        : name(std::move(name))
    {}

    ConfigSelector(Transitivity transitivity)
        : transitivity(transitivity)
    {}

    ConfigSelector(ProjectType projectType)
        : projectType(projectType)
    {}

    ConfigSelector(OperatingSystem targetOS)
        : targetOS(targetOS)
    {}

    ConfigSelector operator+(Transitivity b)
    {
        ConfigSelector a = *this;
        if(a.transitivity) throw std::invalid_argument("Transitivity was specified twice.");
        a.transitivity = b;

        return a;
    }

    ConfigSelector operator+(ProjectType b)
    {
        ConfigSelector a = *this;
        if(a.projectType) throw std::invalid_argument("Project type was specified twice.");
        a.projectType = b;

        return a;
    }

    ConfigSelector operator+(StringId b)
    {
        ConfigSelector a = *this;
        if(a.name) throw std::invalid_argument("Configuration name was specified twice.");
        a.name = b;

        return a;
    }

    ConfigSelector operator+(OperatingSystem b)
    {
        ConfigSelector a = *this;
        if(a.targetOS) throw std::invalid_argument("Configuration target operating system was specified twice.");
        a.targetOS = b;

        return a;
    }

    ConfigSelector operator+(const ConfigSelector& b)
    {
        ConfigSelector a = *this;
        if(b.name) a = a + *b.name;
        if(b.transitivity) a = a + *b.transitivity;
        if(b.projectType) a = a + *b.projectType;
        if(b.targetOS) a = a + *b.targetOS;

        return a;
    }

    std::optional<Transitivity> transitivity;
    std::optional<StringId> name;
    std::optional<ProjectType> projectType;
    std::optional<OperatingSystem> targetOS;

    bool operator <(const ConfigSelector& other) const
    {
        if(transitivity != other.transitivity) return transitivity < other.transitivity;
        if(projectType != other.projectType) return projectType < other.projectType;
        if(name != other.name) return name < other.name;
        if(targetOS != other.targetOS) return targetOS < other.targetOS;

        return false;
    }
};

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

    virtual void applyOverlay(const PropertyBase& other) = 0;
};

template<typename ValueType>
struct Property : public PropertyBase
{
    Property(PropertyGroup* group)
        : PropertyBase(group)
    { }

    template<typename U>
    Property& operator =(U&& v)
    {
        _value = std::forward<U>(v);
        _set = true;
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
  
private:
    void applyOverlay(const PropertyBase& other)
    {
        auto& otherProperty = static_cast<const Property&>(other);
        if(otherProperty._set)
        {
            _value = otherProperty._value;
        }
    }

    ValueType _value{};
    bool _set = false;
};

template<typename ValueType>
struct ListProperty : public PropertyBase
{
    ListProperty(PropertyGroup* group, bool allowDuplicates = false)
        : PropertyBase(group)
        , _allowDuplicates(allowDuplicates)
        , _duplicateTracker(0, IndexedValueHash(_value), IndexedValueEquals(_value))
    { }

    template<typename T>
    ListProperty& operator =(T other)
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
    ListProperty& operator +=(T other) {
        _value.push_back(std::move(other));
        if(!_allowDuplicates)
        {
            if(!_duplicateTracker.insert(_value.size()-1).second)
            {
                _value.pop_back();
            }
        }
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(std::initializer_list<T> other) {
        _value.reserve(_value.size() + other.size());
        for(auto& e : other)
        {
            *this += std::move(e);
        }
        return *this;
    }

    template<typename T>
    ListProperty& operator +=(std::vector<T> other) {
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

    operator const std::vector<ValueType>&() const
    {
        return _value;
    }

    const std::vector<ValueType>& value() const
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

    void applyOverlay(const PropertyBase& other)
    {
        auto& otherListProperty = static_cast<const ListProperty&>(other);
        _value.reserve(_value.size() + otherListProperty.value().size());
        for(auto& e : otherListProperty)
        {
            *this += e;
        }
    }

    bool _allowDuplicates = false;
    std::vector<ValueType> _value{};
    std::unordered_set<int, IndexedValueHash, IndexedValueEquals> _duplicateTracker;
};