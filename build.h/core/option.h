#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "core/stringid.h"

template<typename T>
struct Option : public StringId
{
    using ValueType = T;
};

template<typename T>
struct OptionHash
{
    template<typename U>
    std::enable_if_t<std::is_base_of_v<StringId, U>, size_t> operator()(const U& a) const
    { 
        return std::hash<StringId>()(a);
    };

    template<typename U>
    std::enable_if_t<!std::is_base_of_v<StringId, U>, size_t> operator()(const U& a) const
    { 
        return std::hash<T>()(a);
    };
};

template<>
struct OptionHash<std::filesystem::path>
{
    size_t operator()(const std::filesystem::path& a) const
    { 
        return std::filesystem::hash_value(a);
    };
};

struct OptionStorage
{
    using Data = std::unique_ptr<void, void(*)(const void*)>;

    OptionStorage()
        : _data{nullptr, &OptionStorage::nullDeleter}
    {
    }

    template<typename T> 
    T& get() const
    {
        if(!_data)
        {
            static T empty;
            return empty;
        }
        return *static_cast<T*>(_data.get());
    }

    template<typename T> 
    T& getOrAdd()
    {
        if(!_data)
        {
            static auto deleter = [](const void* data)
            {
                delete static_cast<const T*>(data);
            };
            _data = Data(new T{}, deleter);

            static auto cloner = [](const OptionStorage& b)
            {
                OptionStorage clone;
                clone.getOrAdd<T>() = b.get<T>();
                return clone;
            };
            _cloner = cloner;

            static auto combiner = [](OptionStorage& a, const OptionStorage& b)
            {
                combineValues(a.getOrAdd<T>(), b.get<T>());
            };
            _combiner = combiner;

            static auto deduplicator = [](OptionStorage& a)
            {
                deduplicateValues(a.get<T>());
            };
            _deduplicator = deduplicator;
        }
        return *static_cast<T*>(_data.get());
    }

    void combine(const OptionStorage& other)
    {
        _combiner(*this, other);
    }

    void deduplicate()
    {
        _deduplicator(*this);
    }

    OptionStorage clone() const
    {
        return _cloner(*this);
    }

private:
    template<typename U>
    static void combineValues(U& a, U b)
    {
        a = b;
    }

    template<typename U, typename V>
    static void combineValues(std::map<U, V>& a, std::map<U, V> b)
    {
        a.merge(b);
    }

    template<typename U>
    static void combineValues(std::vector<U>& a, std::vector<U> b)
    {
        a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
    }

    template<typename U>
    static void deduplicateValues(U& v)
    {
    }

    template<typename U>
    static void deduplicateValues(std::vector<U>& v)
    {
        // Tested a few methods and this was the fastest one I came up with that's also pretty simple

        // Could probably also use a custom insertion ordered set instead of vectors to hold options
        // from the start, but this was simpler (and some quick tests indicated possibly faster)
 
        struct DerefEqual
        {
            bool operator ()(const U* a, const U* b) const
            {
                return *a == *b;
            }
        };

        struct DerefHash
        {
            size_t operator ()(const U* a) const
            {
                return OptionHash<U>()(*a);
            }
        };

        std::unordered_set<const U*, DerefHash, DerefEqual> dups;
        dups.reserve(v.size());
        v.erase(std::remove_if(v.begin(), v.end(), [&dups](const U& a) { 
            return !dups.insert(&a).second;
        }), v.end());
    }

    static void nullDeleter(const void*) {}

    OptionStorage(*_cloner)(const OptionStorage&);
    void(*_combiner)(OptionStorage&, const OptionStorage&);
    void(*_deduplicator)(OptionStorage&);
    Data _data;
};

struct OptionCollection
{
    template<typename T>
    T& operator[](Option<T> option)
    {
        return _storage[option].template getOrAdd<T>();
    }

    OptionCollection& operator+=(const OptionCollection& other)
    {
        combine(other);
        return *this;
    }

    void combine(const OptionCollection& other)
    {
        for(auto& entry : other._storage)
        {
            auto it = _storage.find(entry.first);
            if(it != _storage.end())
            {
                it->second.combine(entry.second);
            }
            else
            {
                _storage[entry.first] = entry.second.clone();
            }
        }
    }

    void deduplicate()
    {
        for(auto& entry : _storage)
        {
            entry.second.deduplicate();
        }
    }

private:
    std::map<const char*, OptionStorage> _storage;
};