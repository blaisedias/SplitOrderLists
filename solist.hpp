/*

Copyright (C) 2017,2018  Blaise Dias

sharedobj is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sharedobj.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BENEDIAS_SOLIST_HPP
#define BENEDIAS_SOLIST_HPP
#include <atomic>
#include <cassert>
#include <cstdint>
#include <utility>
#include <memory>
#include "mark_ptr_type.hpp"
#if 1
#include <iostream>
#include <cstdio>
#endif

namespace benedias {
    namespace concurrent {

    // FIXME: for the moment this module uses 32bit hashes
    using hash_t = uint32_t;
    using so_key = uint32_t;
    const   hash_t      DATABIT = 0x1;
    hash_t reverse_hasht_bits(hash_t hashv);

    // FIXME: @insert this effectively reduces the hash space for reverse
    // hashes by half, since 1 bit is "lost" by virtue overwritten, increasing
    // the likelihood fo collisions. This could be alleviated at a cost in space
    // and execution time by storing the original hash value in the node.
    inline so_key sol_node_key(hash_t hashv)
    {
        return reverse_hasht_bits(hashv) | DATABIT;
    }

    // FIXME: handle the error condition more gracefully than an assert.
    inline so_key sol_bucket_key(hash_t hashv)
    {
        hash_t bucket_key = reverse_hasht_bits(hashv);
        assert(0 == (bucket_key & DATABIT));
        return bucket_key;
    }

    class solist_bucket
    {
        protected:
        // Non copyable
        solist_bucket& operator=(const solist_bucket&) = delete;
        solist_bucket(solist_bucket const&) = delete;

        // Non movable
        solist_bucket& operator=(solist_bucket&&) = delete;
        solist_bucket(solist_bucket&&) = delete;

        solist_bucket() {}

        public:
        hash_t          hashv;
        so_key          key;
        mark_ptr_type<solist_bucket>  next;

        explicit solist_bucket(hash_t hashv):hashv(hashv),key(sol_bucket_key(hashv)){}
        inline bool is_node()
        {
            return DATABIT == (key & DATABIT);
        }
        virtual ~solist_bucket() = default;
    };

    template <typename T> struct solist_node: solist_bucket
    {
        T               payload;

        // Non copyable
        solist_node& operator=(const solist_node&) = delete;
        solist_node(solist_node const&) = delete;

        // Non movable
        solist_node& operator=(solist_node&&) = delete;
        solist_node(solist_node&&) = delete;

        explicit solist_node(T data, hash_t hashv):solist_bucket(hashv),payload(data)
        {
            key |= DATABIT;
        }
        T*              get_item_ptr() { return &payload; }
        ~solist_node() = default;

    };

#if 0
    template <typename T> class solist_traverse
    {
        solist_bucket* hp[3];
        public:
        inline solist_bucket* next() { return hp[0]; }
        inline solist_bucket* cur() { return hp[1]; }
        inline solist_bucket* prev() { return hp[1]; }
    };
#endif

    template <typename T> struct solist
    {
        uint32_t            size;
        uint32_t            max_bucket_length = 4;
        uint32_t            n_items = 0;
        std::unique_ptr<solist_bucket*[]>     buckets = nullptr;

        // Non copyable
        solist& operator=(const solist&) = delete;
        solist(solist const&) = delete;

        // Non movable
        solist& operator=(solist&&) = delete;
        solist(solist&&) = delete;

        explicit solist(uint32_t size):size(size),buckets(new solist_bucket*[size])
        {
            for(uint32_t x=0; x < size; ++x)
            {
                buckets[x] = nullptr;
            }
            buckets[0] = new solist_bucket(0);
        }

        inline void inc_item_count()
        {
            __atomic_add_fetch(&n_items, 1, __ATOMIC_RELEASE); 
        }

        inline void dec_item_count()
        {
            __atomic_sub_fetch(&n_items, 1, __ATOMIC_RELEASE); 
        }

        explicit solist(uint32_t size, uint32_t bucket_length):size(size),max_bucket_length(bucket_length),buckets(new solist_bucket*[size])
        {
            for(uint32_t x=0; x < size; ++x)
            {
                buckets[x] = nullptr;
            }
            buckets[0] = new solist_bucket(0);
        }

        ~solist()
        {
            solist_bucket* cur = buckets[0];
            solist_bucket* next;

            while(nullptr != cur)
            {
                next = cur->next();
                delete cur;
                cur = next;
            }
        }

        void expand(uint32_t curr_size)
        {
            if (curr_size < size)
            {
                return;
            }
            uint32_t new_size = size * 2;
            auto new_buckets = std::make_unique<solist_bucket*[]>(new_size);
            for (uint32_t x=0; x < size; ++x)
            {
                new_buckets[x] = buckets[x];
            }
            for (uint32_t x=size; x < new_size; ++x)
            {
                new_buckets[x] = nullptr;
            }

            //FIXME: update of buckets and size fields has to be combined atomically :-(
            //Solution 1: ugly! :-(
            // add another level of indirection, solist will be a wrapper around another
            // class which has the buckets and size fields.
            // indirection has performance impact.
            buckets.swap(new_buckets);
            size = new_size;
        }
    };

#if 0
    template <typename T> class solist_accessor;
    template <typename T> void dump_solist_buckets(solist_accessor<T>& sol);
    template <typename T> void dump_solist_keys(solist_accessor<T>& sol);
    template <typename T> void dump_solist_key_order(solist_accessor<T>& sol);
    template <typename T> void dump_solist(solist_accessor<T>& sol);
    template <typename T> void dump_solist_items(solist_accessor<T>& sol);
    template <typename T> void check_solist(solist_accessor<T>& sol);
#endif

    template <typename T> class solist_accessor
    {
        std::shared_ptr<solist<T>> so_list;

        solist_bucket *cur;
        solist_bucket *next;
        solist_bucket *prev;
        unsigned    steps;

#if 0
        friend void dump_solist_buckets(solist_accessor<T>& sol);
        friend void dump_solist_keys(solist_accessor<T>& sol);
        friend void dump_solist_key_order(solist_accessor<T>& sol);
        friend void dump_solist(solist_accessor<T>& sol);
        friend void dump_solist_items(solist_accessor<T>& sol);
        friend void check_solist(solist_accessor<T>& sol);
#else
        template <typename U> friend void dump_solist_buckets(solist_accessor<U>& sol);
        template <typename U> friend void dump_solist_keys(solist_accessor<U>& sol);
        template <typename U> friend void dump_solist_key_order(solist_accessor<U>& sol);
        template <typename U> friend void dump_solist(solist_accessor<U>& sol);
        template <typename U> friend void dump_solist_items(solist_accessor<U>& sol);
        template <typename U> friend void check_solist(solist_accessor<U>& sol);
#endif       

        inline void advance()
        {
            // TODO:  setup hazard pointers here in the required order
            // and delete nodes traversed which marked for delete,
            prev = cur;
            cur = next;
            if (nullptr != cur)
                next = cur->next();
        }

        inline void zap()
        {
            //TODO: clear the hazard pointers.
            prev = cur = next = nullptr;
        }

        void hazp_init()
        {
            // TODO: Acquire a block of 3 hazard pointers.
            zap();
        }

        public:
        solist_accessor& operator=(const solist_accessor& other)
        {
            so_list = other.so_list;
            hazp_init();
        }

        solist_accessor(solist_accessor const& other)
        {
            so_list = other.so_list;
            hazp_init();
        }

        solist_accessor(std::shared_ptr<solist<T>> sl):so_list(sl)
        {
            hazp_init();
        }

        explicit solist_accessor(uint32_t size)
        {
            so_list = std::make_shared<solist<T>>(size);
            hazp_init();
        }

        explicit solist_accessor(uint32_t size, uint32_t bucket_length)
        {
            so_list = std::make_shared<solist<T>>(size, bucket_length);
            hazp_init();
        }


        ~solist_accessor()=default;

        private:
        void get_parent(uint32_t slot, so_key key)
        {
            prev = cur = so_list->buckets[0];

            while(slot-- != 0)
            {
                if (so_list->buckets[slot])
                {
                    prev = cur = so_list->buckets[slot];
                }
            }

            next = cur->next();

            while(nullptr != next && next->key < key)
            {
                advance();
            }
        }

        public:
        void initialise_bucket(hash_t slot)
        {
            assert(slot < so_list->size);

            if (so_list->buckets[slot] != nullptr)
            {
                return;
            }

            auto node = new solist_bucket(slot);
            so_key key = node->key;
            do
            {
                get_parent(slot, key);
                // cur is the node after which to insert dummy node.
                node->next = next;
            }while (
                    // a.n.other thread successfully has initialised
                    // the bucket.
                    nullptr == so_list->buckets[slot]
                    // a.n.other thread successfully inserted its instance of
                    // the dummy node.
                    && (nullptr == next || next->key != key)
                    // this will fail if the relevant elements of the list
                    // changed after calling get_parent
                    && (!cur->next.CAS(next, node)));

            if (so_list->buckets[slot] == nullptr)
            {
                if(cur->next() == node)
                {
                    // success!
                    so_list->buckets[slot] = node;
                    next = node;
                }
                else
                {
                    // a.n.other thread inserted its instance of the bucket node.
                    // Setup the slot correctly to point to that instance,
                    // so the bucket is guaranteed 
                    // to be initialised on return.
                    so_list->buckets[slot] = next;
                    assert(so_list->buckets[slot]->key == key);
                    delete node;
                }
            }
            else
            {
                // a.n.other thread has already initialised the bucket.
                delete node;
            }

            assert(nullptr != so_list->buckets[slot]);
            assert(so_list->buckets[slot]->key == key);
        }

        private:
        bool find_node(hash_t hashv)
        {
            uint32_t slot = hashv % so_list->size;
            so_key key = sol_node_key(hashv);

            if(so_list->buckets[slot] == nullptr)
            {
                // lazy initialisation of a bucket
                initialise_bucket(slot);
            }
            
            prev = cur = so_list->buckets[slot];
            next = cur->next();

            steps = 0;
            while((nullptr != next) && (next->key <= key))
            {
                advance();
                ++steps;
            }

            if((nullptr == cur) || (cur->key != key))
            {
                return false;
            }
            assert(cur->key == key);
            return true;
        }

        void speculative_expand(hash_t hashv)
        {
            uint32_t slot = hashv % so_list->size;
            uint32_t span = 0;
            uint32_t    nbuckets = so_list->size;

            if(so_list->buckets[slot] == nullptr)
            {
                initialise_bucket(slot);
            }
            
            prev = cur = so_list->buckets[slot];
            next = cur->next();

            while(nullptr != next && next->is_node())
            {
                advance();
                ++span;
            }

            if (span >= so_list->max_bucket_length)
            {
                // expand if
                // 1) the bucket is full by a factor of 2 FIXME (make the factor configurable) 
                //      this can happen for pathological insert sequences where
                //      inserts are to the same bucket repeatedly.
                // 2) the all buckets are full
                if (
                        (span >= so_list->max_bucket_length * 2)
                        ||
                        (so_list->n_items >= (so_list->max_bucket_length * so_list->size))
                   )
                {
                    so_list->expand(nbuckets);
                }
                {
                    span = 0;
                    prev = cur = so_list->buckets[slot];
                    next = cur->next();
                    while(nullptr != next && next->is_node())
                    {
                        if (so_list->buckets[slot] == nullptr)
                        {
                            span = 0;
                            initialise_bucket(cur->hashv % so_list->size);
                        }
                        advance();
                        ++span;
                    }
                }
            }
        }

        public:
        // insert is the most expensive operation because
        // it is the best location to amortise some of the 
        // cost of automatic expanding the number of buckets.
        // FIXME: explore using bucket item counters,
        // complexity getting the counts correct on bucket split.
        bool insert_node(hash_t hashv, T payload)
        {
            bool result = false;
            uint32_t    nbuckets = so_list->size;
            auto dnode = new solist_node<T>(payload, hashv);

//            speculative_expand(hashv);
            while(true)
            {
                if(find_node(hashv))
                {
                    break;
                }
                
                dnode->next = next;
                if(cur->next.CAS(next, dnode))
                {
                    so_list->inc_item_count();
                    result = true;
                    break;
                }
            }

            if (!result)
            {
                delete dnode;
            }
            else
            {
                // !!!WARNING!!!  if we need to initialise a hazard pointer to the
                // newly added node then we should set that before proceeding with
                // the expansion check.

                next = cur->next();
                //
                // added a node, so do expansion check.
                while(nullptr != next && next->is_node())
                {
                    advance();
                    ++steps;
                }

                if(steps > so_list->max_bucket_length)
                {
                    // Record the bucket number before expansion.
                    uint32_t slot = hashv % so_list->size;
                    // expand if
                    // 1) the bucket is overflows by a factor of 2 FIXME (make the factor configurable) 
                    //      this can happen for pathological insert sequences where
                    //      inserts are to the same bucket repeatedly.
                    // 2) the all buckets are full
                    if (
                            (steps >= ((so_list->max_bucket_length * 2)))
                            ||
                            (so_list->n_items >= (so_list->max_bucket_length * so_list->size))
                       )
                    {
                        so_list->expand(nbuckets);
                        initialise_bucket(slot + nbuckets);
                    }
                    else
                    {
                        // split the bucket we inserted into when a bucket
                        // "overflows", this is only effective if the bucket
                        // was not split following an expand.
                        initialise_bucket(slot + (nbuckets/2));
                    }
                }
            }
            zap();
            return result;
        }

        bool delete_node(hash_t hashv)
        {
            bool result = false;

            while(true)
            {
                if(!find_node(hashv))
                {
                    break;
                }
                
                // Mark
                if(!cur->next.CAS(next, next, true))
                {
                    continue;
                }

                // remove
                if(prev->next.CAS(cur, next))
                {
                   so_list->dec_item_count();
                   delete cur; 
                   result = true;
                   break;
                }
            }

            zap();
            return result;
        }

        // FIXME: for proper operation we should return type hazard_pointer<T>
        // TBD.
        T* find_item_node(hash_t hashv)
        {
            if (find_node(hashv))
            {
                // can make cheaper using reinterpret_cast for now this is safer,
                // but more expensive.
                solist_node<T>* node = dynamic_cast<solist_node<T>*>(cur);
                return node->get_item_ptr();
            }
        }
    };

    } //namespace concurrent
} //namespace benedias
#endif // #define BENEDIAS_SOLIST_HPP
