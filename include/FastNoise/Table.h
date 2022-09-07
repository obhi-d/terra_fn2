
#pragma once

#include <cstdint>
#include <vector>

namespace FastNoise
{
    template<typename T>
    class Table
    {
    public:
        inline static T null;

        T& At( std::int32_t i )
        {
            if( i < 0 )
                return null;
            if( (size_t)i < indexes.size() && (size_t)indexes[i] < items.size() )
                return items[indexes[i]].first;
            return null;
        }

        T const& At( std::int32_t i ) const
        {
            if( i < 0 )
                return null;
            return items[indexes[i]].first;
        }

        std::int32_t Emplace( T&& other )
        {
            int id = free;
            if( free >= 0 )
            {
                free          = indexes[free];
                indexes[free] = (int)items.size();
            }
            else
            {
                id = (int)indexes.size();
                indexes.emplace_back( (int)items.size() );
            }
            items.emplace_back( std::move( other ), id );
            return id;
        }

        void Erase( std::int32_t item )
        {
            auto emptySlot = indexes[item];
            if( items.size() > 1 )
            {
                auto repoint     = items.back().second;
                items[emptySlot] = std::move( items.back() );
                indexes[repoint] = emptySlot;
                items.pop_back();
                indexes[item] = free;
                free          = item;
            }
            else
            {
                items.clear();
                indexes.clear();
                free = -1;
            }
        }


        std::vector<std::pair<T, std::int32_t>> items;
        std::vector<std::int32_t>               indexes;
        std::int32_t                            free = -1;
    };
} // namespace FastNoise
