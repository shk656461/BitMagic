#ifndef BMAGGREGATOR__H__INCLUDED__
#define BMAGGREGATOR__H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/*! \file bmaggregator.h
    \brief Algorithms for fast aggregation of N bvectors
*/

#include "bm.h"
#include "bmfunc.h"
#include "bmdef.h"

#include "bmalgo_impl.h"


namespace bm
{


/**
    Algorithms for fast aggregation of a group of bit-vectors
 
    Current implementation can aggregate up to max_aggregator_cap vectors
    (TODO: remove this limitation)
 
    Algorithms of this class use cache locality optimizations and efficient
    on cases, wehen we need to apply the same logical operation (aggregate)
    more than 2x vectors.
 
    TARGET = BV1 or BV2 or BV3 or BV4 ...
 
    \ingroup setalgo
*/
template<typename BV>
class aggregator
{
public:
    typedef BV                         bvector_type;
    typedef const bvector_type*        bvector_type_const_ptr;
    typedef bm::id64_t                 digest_type;
    
    typedef typename bvector_type::allocator_type::allocator_pool_type allocator_pool_type;

    /// Maximum aggregation capacity in one pass
    enum max_size
    {
        max_aggregator_cap = 256
    };

    
public:
    aggregator();
    ~aggregator();
    
    // -----------------------------------------------------------------------
    
    /*! @name Methods to setup argument groups and run operations on groups */
    //@{
    
    /**
        Attach source bit-vector to a argument group (0 or 1).
        Arg group 1 used for fused operations like (AND-SUB)
        \param bv - input bit-vector pointer to attach
        \param agr_group - input argument group (0 - default, 1 - fused op)
     
        @return current arg group size (0 if vector was not added (empty))
        @sa reset
    */
    unsigned add(const bvector_type* bv, unsigned agr_group = 0);
    
    /**
        Reset aggregate groups, forget all attached vectors
    */
    void reset() { arg_group0_size = arg_group1_size = 0; }

    /**
        Aggregate added group of vectors using logical OR
        Operation does NOT perform an explicit reset of arg group(s)
     
        \param bv_target - target vector (input is arg group 0)
     
        @sa add, reset
    */
    void combine_or(bvector_type& bv_target);

    /**
        Aggregate added group of vectors using logical AND
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0)
     
        @sa add, reset
    */
    void combine_and(bvector_type& bv_target);

    /**
        Aggregate added group of vectors using fused logical AND-SUB
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0(AND), 1(SUB) )
     
        \return true if anything was found
     
        @sa add, reset
    */
    bool combine_and_sub(bvector_type& bv_target);
    
    
    /**
        Aggregate added group of vectors using fused logical AND-SUB
        Operation does NOT perform an explicit reset of arg group(s)
        Operation can terminate early if anything was found.

        \param bv_target - target vector (input is arg group 0(AND), 1(SUB) )
        \param any - if true, search result will terminate of first found result

        \return true if anything was found

        @sa add, reset
    */
    bool combine_and_sub(bvector_type& bv_target, bool any);
    
    bool find_first_and_sub(bm::id_t& idx);


    /**
        Aggregate added group of vectors using SHIFT-RIGHT and logical AND
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0)
     
        @return bool if anything was found
     
        @sa add, reset
    */
    void combine_shift_right_and(bvector_type& bv_target);

    //@}
    
    // -----------------------------------------------------------------------
    
    /*! @name Logical operations (C-style interface) */
    //@{

    /**
        Aggregate group of vectors using logical OR
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_or(bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, unsigned src_size);

    /**
        Aggregate group of vectors using logical AND
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_and(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, unsigned src_size);

    /**
        Fusion aggregate group of vectors using logical AND MINUS another set
     
        \param bv_target     - target vector
        \param bv_src_and    - array of pointers on bit-vectors for AND
        \param src_and_size  - size of AND group
        \param bv_src_sub    - array of pointers on bit-vectors for SUBstract
        \param src_sub_size  - size of SUB group
        \param any           - flag if caller needs any results asap (incomplete results)
     
        \return true when found
    */
    bool combine_and_sub(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                     const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size,
                     bool any);
    
    bool find_first_and_sub(bm::id_t& idx,
                     const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                     const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size);

    /**
        Fusion aggregate group of vectors using SHIFT right with AND
     
        \param bv_target     - target vector
        \param bv_src_and    - array of pointers on bit-vectors for AND masking
        \param src_and_size  - size of AND group
        \param any           - flag if caller needs any results asap (incomplete results)
     
        \return true when found
    */
    bool combine_shift_right_and(bvector_type& bv_target,
            const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
            bool any);


    /**
        Fusion shift right, then AND with mask vector

        \param bv_target     - target vector (it is getting shifted)
        \param bv_mask       - AND mask vector
     
        \return any true when result found (false if target gets empty)
    */
    bool shift_right_and(bvector_type& bv_target, const bvector_type& bv_mask);
    
    //@}

    // -----------------------------------------------------------------------

    /*! @name Horizontal Logical operations used for tests (C-style interface) */
    //@{
    
    /**
        Horizontal OR aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_or_horizontal(bvector_type& bv_target,
                               const bvector_type_const_ptr* bv_src, unsigned src_size);
    /**
        Horizontal AND aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_and_horizontal(bvector_type& bv_target,
                                const bvector_type_const_ptr* bv_src, unsigned src_size);

    /**
        Horizontal AND-SUB aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src_and    - array of pointers on bit-vector to AND aggregate
        \param src_and_size  - size of bv_src_and
        \param bv_src_sub    - array of pointers on bit-vector to SUB aggregate
        \param src_sub_size  - size of bv_src_sub
    */
    void combine_and_sub_horizontal(bvector_type& bv_target,
                                    const bvector_type_const_ptr* bv_src_and,
                                    unsigned src_and_size,
                                    const bvector_type_const_ptr* bv_src_sub,
                                    unsigned src_sub_size);

    //@}

protected:
    typedef typename bvector_type::blocks_manager_type blocks_manager_type;
    
    void combine_or(unsigned i, unsigned j,
                    bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, unsigned src_size);

    void combine_and(unsigned i, unsigned j,
                    bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, unsigned src_size);
    
    digest_type combine_and_sub(unsigned i, unsigned j,
                         const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                         const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size);
    
    bool combine_shift_right_and(unsigned i, unsigned j,
                                 bvector_type& bv_target,
                                 const bvector_type_const_ptr* bv_src, unsigned src_size);


    static
    unsigned resize_target(bvector_type& bv_target,
                           const bvector_type_const_ptr* bv_src,
                           unsigned src_size,
                           bool init_clear = true);
    
    bm::word_t* sort_input_blocks_or(const bvector_type_const_ptr* bv_src,
                                     unsigned src_size,
                                     unsigned i, unsigned j,
                                     unsigned* arg_blk_count,
                                     unsigned* arg_blk_gap_count);
    
    bm::word_t* sort_input_blocks_and(const bvector_type_const_ptr* bv_src,
                                      unsigned src_size,
                                      unsigned i, unsigned j,
                                      unsigned* arg_blk_count,
                                      unsigned* arg_blk_gap_count);

    bool process_bit_blocks_or(blocks_manager_type& bman_target,
                               unsigned i, unsigned j, unsigned block_count);

    bool process_gap_blocks_or(blocks_manager_type& bman_target,
                               unsigned i, unsigned j, unsigned block_count);
    
    digest_type process_bit_blocks_and(unsigned block_count, digest_type digest);
    
    digest_type process_gap_blocks_and(unsigned block_count, digest_type digest);

    digest_type process_bit_blocks_sub(unsigned block_count, digest_type digest);

    digest_type process_gap_blocks_sub(unsigned block_count, digest_type digest);

    static
    unsigned find_effective_sub_block_size(unsigned i,
                                           const bvector_type_const_ptr* bv_src,
                                           unsigned src_size);
    
    bool any_carry_overs(unsigned co_size) const;
    
private:
    /// Memory arena for logical operations
    ///
    /// @internal
    struct arena
    {
        BM_DECLARE_TEMP_BLOCK(tb1);
        // commented out blocks are used in experimental GAP aggregations
#if 0
        bm::gap_word_t        gap_res_buf1[bm::gap_equiv_len * 3]; ///< temp 1
        bm::gap_word_t        gap_res_buf2[bm::gap_equiv_len * 3]; ///< temp 2
        bm::gap_word_t        gap_res_buf3[bm::gap_equiv_len * 6]; ///< temp 3
#endif
        const bm::word_t*     v_arg_blk[max_aggregator_cap];     ///< source blocks list
        const bm::gap_word_t* v_arg_blk_gap[max_aggregator_cap]; ///< source GAP blocks list
        
        bvector_type_const_ptr arg_bv0[max_aggregator_cap]; ///< arg group 0
        bvector_type_const_ptr arg_bv1[max_aggregator_cap]; ///< arg group 1
        unsigned char          carry_overs_[max_aggregator_cap]; /// carry over flags
    };
    
    aggregator(const aggregator&) = delete;
    aggregator& operator=(const aggregator&) = delete;
    
private:
    arena*          ar_; ///< data arena ptr (heap allocated)
    unsigned        arg_group0_size = 0;
    unsigned        arg_group1_size = 0;
    allocator_pool_type  pool_; ///< pool for operations with cyclic mem.use
};


// ------------------------------------------------------------------------
//
// ------------------------------------------------------------------------


template<typename BV>
aggregator<BV>::aggregator()
{
    ar_ = (arena*) bm::aligned_new_malloc(sizeof(arena));
}

// ------------------------------------------------------------------------

template<typename BV>
aggregator<BV>::~aggregator()
{
    BM_ASSERT(ar_);
    bm::aligned_free(ar_);
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned aggregator<BV>::add(const bvector_type* bv, unsigned agr_group)
{
    BM_ASSERT_THROW(agr_group <= 1, BM_ERR_RANGE);
    BM_ASSERT(agr_group <= 1);
    
    if (agr_group) // arg group 1
    {
        BM_ASSERT(arg_group1_size < max_aggregator_cap);
        BM_ASSERT_THROW(arg_group1_size < max_aggregator_cap, BM_ERR_RANGE);
        
        if (!bv)
            return arg_group1_size;
        
        ar_->arg_bv1[arg_group1_size++] = bv;
        return arg_group1_size;
    }
    else // arg group 0
    {
        BM_ASSERT(arg_group0_size < max_aggregator_cap);
        BM_ASSERT_THROW(arg_group0_size < max_aggregator_cap, BM_ERR_RANGE);

        if (!bv)
            return arg_group0_size;

        ar_->arg_bv0[arg_group0_size++] = bv;
        return arg_group0_size;
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(bvector_type& bv_target)
{
    combine_or(bv_target, ar_->arg_bv0, arg_group0_size);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(bvector_type& bv_target)
{
    combine_and(bv_target, ar_->arg_bv0, arg_group0_size);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target)
{
    return combine_and_sub(bv_target,
                    ar_->arg_bv0, arg_group0_size,
                    ar_->arg_bv1, arg_group1_size, false);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target, bool any)
{
    return combine_and_sub(bv_target,
                    ar_->arg_bv0, arg_group0_size,
                    ar_->arg_bv1, arg_group1_size, any);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::find_first_and_sub(bm::id_t& idx)
{
    return find_first_and_sub(idx,
                        ar_->arg_bv0, arg_group0_size,
                        ar_->arg_bv1, arg_group1_size);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_shift_right_and(bvector_type& bv_target)
{
    combine_shift_right_and(bv_target, ar_->arg_bv0, arg_group0_size, false);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(bvector_type& bv_target,
                        const bvector_type_const_ptr* bv_src, unsigned src_size)
{
    BM_ASSERT_THROW(src_size < max_aggregator_cap, BM_ERR_RANGE);
    if (!src_size)
    {
        bv_target.clear();
        return;
    }

    unsigned top_blocks = resize_target(bv_target, bv_src, src_size);
    for (unsigned i = 0; i < top_blocks; ++i)
    {
        unsigned set_array_max = find_effective_sub_block_size(i, bv_src, src_size);
        unsigned j = 0;
        do
        {
            combine_or(i, j, bv_target, bv_src, src_size);
            ++j;
        } while (j < set_array_max);
    } // for i
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(bvector_type& bv_target,
                        const bvector_type_const_ptr* bv_src, unsigned src_size)
{
    BM_ASSERT_THROW(src_size < max_aggregator_cap, BM_ERR_RANGE);

    if (!src_size)
    {
        bv_target.clear();
        return;
    }

    unsigned top_blocks = resize_target(bv_target, bv_src, src_size);
    
    for (unsigned i = 0; i < top_blocks; ++i)
    {
        unsigned set_array_max = find_effective_sub_block_size(i, bv_src, src_size);
        unsigned j = 0;
        do
        {
            combine_and(i, j, bv_target, bv_src, src_size);
            ++j;
        } while (j < set_array_max);
    } // for i
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target,
                 const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                 const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size,
                 bool any)
{
    BM_ASSERT_THROW(src_and_size < max_aggregator_cap, BM_ERR_RANGE);
    BM_ASSERT_THROW(src_sub_size < max_aggregator_cap, BM_ERR_RANGE);
    
    bool global_found = false;

    if (!bv_src_and || !src_and_size)
    {
        bv_target.clear();
        return false;
    }

    unsigned top_blocks = resize_target(bv_target, bv_src_and, src_and_size);
    unsigned top_blocks2 = resize_target(bv_target, bv_src_sub, src_sub_size, false);
    
    if (top_blocks2 > top_blocks)
        top_blocks = top_blocks2;

    for (unsigned i = 0; i < top_blocks; ++i)
    {
        unsigned set_array_max = find_effective_sub_block_size(i, bv_src_and, src_and_size);
        if (src_sub_size)
        {
            unsigned set_array_max2 =
                    find_effective_sub_block_size(i, bv_src_sub, src_sub_size);
            if (set_array_max2 > set_array_max)
                set_array_max = set_array_max2;
        }
        unsigned j = 0;
        do
        {
            digest_type digest = combine_and_sub(i, j,
                                                bv_src_and, src_and_size,
                                                bv_src_sub, src_sub_size);
            bool found = digest;
            if (found)
            {
                blocks_manager_type& bman_target = bv_target.get_blocks_manager();
                bman_target.copy_bit_block(i, j, ar_->tb1);
                if (any)
                    return found;
            }
            global_found |= found;
            ++j;
        } while (j < set_array_max);
    } // for i
    return global_found;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::find_first_and_sub(bm::id_t& idx,
                 const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                 const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size)
{
    BM_ASSERT_THROW(src_and_size < max_aggregator_cap, BM_ERR_RANGE);
    BM_ASSERT_THROW(src_sub_size < max_aggregator_cap, BM_ERR_RANGE);
    
    if (!bv_src_and || !src_and_size)
        return false;

    BV bv_target; // TODO: get rid of it
    unsigned top_blocks = resize_target(bv_target, bv_src_and, src_and_size);
    unsigned top_blocks2 = resize_target(bv_target, bv_src_sub, src_sub_size, false);
    
    if (top_blocks2 > top_blocks)
        top_blocks = top_blocks2;

    for (unsigned i = 0; i < top_blocks; ++i)
    {
        unsigned set_array_max = find_effective_sub_block_size(i, bv_src_and, src_and_size);
        if (src_sub_size)
        {
            unsigned set_array_max2 =
                    find_effective_sub_block_size(i, bv_src_sub, src_sub_size);
            if (set_array_max2 > set_array_max)
                set_array_max = set_array_max2;
        }
        unsigned j = 0;
        do
        {
            digest_type digest = combine_and_sub(i, j,
                                                bv_src_and, src_and_size,
                                                bv_src_sub, src_sub_size);
            if (digest)
            {
                bool found = bm::bit_find_first(ar_->tb1, &idx);
                BM_ASSERT(found);
                unsigned base_idx = i * bm::set_array_size * bm::gap_max_bits;
                base_idx += j * bm::gap_max_bits;
                idx += base_idx;
                return found;
            }
            ++j;
        } while (j < set_array_max);
    } // for i
    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned
aggregator<BV>::find_effective_sub_block_size(unsigned i,
                                              const bvector_type_const_ptr* bv_src,
                                              unsigned src_size) 
{
    unsigned max_size = 1;
    for (unsigned k = 0; k < src_size; ++k)
    {
        const bvector_type* bv = bv_src[k];
        BM_ASSERT(bv);
        const typename bvector_type::blocks_manager_type& bman_arg = bv->get_blocks_manager();

        const bm::word_t* const* blk_blk_arg = bman_arg.get_topblock(i);
        if (!blk_blk_arg)
            continue;

        for (unsigned j = bm::set_array_size - 1; j > max_size; --j)
        {
            if (blk_blk_arg[j])
            {
                max_size = j; break;
            }
        }
    } // for k
    ++max_size;
    BM_ASSERT(max_size <= bm::set_array_size);
    
    return max_size;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(unsigned i, unsigned j,
                                bvector_type& bv_target,
                                const bvector_type_const_ptr* bv_src,
                                unsigned src_size)
{
    typename bvector_type::blocks_manager_type& bman_target = bv_target.get_blocks_manager();

    unsigned arg_blk_count = 0;
    unsigned arg_blk_gap_count = 0;
    bm::word_t* blk =
        sort_input_blocks_or(bv_src, src_size, i, j,
                             &arg_blk_count, &arg_blk_gap_count);

    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);

    if (blk == FULL_BLOCK_FAKE_ADDR) // nothing to do - golden block(!)
    {
        bman_target.check_alloc_top_subblock(i);
        bman_target.set_block_ptr(i, j, blk);
    }
    else
    {
        blk = ar_->tb1;
        if (arg_blk_count || arg_blk_gap_count)
        {
            bool all_one =
                process_bit_blocks_or(bman_target, i, j, arg_blk_count);
            if (!all_one)
            {
                if (arg_blk_gap_count)
                {
                    all_one =
                        process_gap_blocks_or(bman_target, i, j, arg_blk_gap_count);
                }
                if (!all_one)
                {
                    // we have some results, allocate block and copy from temp
                    bman_target.copy_bit_block(i, j, ar_->tb1);
                }
            }
        }
    }

}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(unsigned i, unsigned j,
                                 bvector_type& bv_target,
                                 const bvector_type_const_ptr* bv_src,
                                 unsigned src_size)
{
    BM_ASSERT(src_size);
    
    typename bvector_type::blocks_manager_type& bman_target = bv_target.get_blocks_manager();

    unsigned arg_blk_count = 0;
    unsigned arg_blk_gap_count = 0;
    bm::word_t* blk =
        sort_input_blocks_and(bv_src, src_size,
                              i, j,
                              &arg_blk_count, &arg_blk_gap_count);

    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);

    if (!blk) // nothing to do - golden block(!)
    {
    }
    else
    {
        if (arg_blk_count || arg_blk_gap_count)
        {
            // AND bit-blocks
            //
            bm::id64_t digest = 0;
            digest = process_bit_blocks_and(arg_blk_count, digest);
            if (!digest)
                return;

            // AND all GAP blocks (if any)
            //
            if (arg_blk_gap_count)
            {
                digest =
                    process_gap_blocks_and(arg_blk_gap_count, digest);
            }
            
            if (digest) // some results
            {
                // we have some results, allocate block and copy from temp
                bman_target.copy_bit_block(i, j, ar_->tb1);
            }

        }
    }

}
// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::combine_and_sub(unsigned i, unsigned j,
                     const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                     const bvector_type_const_ptr* bv_src_sub, unsigned src_sub_size)
{
    BM_ASSERT(src_and_size);

    unsigned arg_blk_and_count = 0;
    unsigned arg_blk_and_gap_count = 0;
    unsigned arg_blk_sub_count = 0;
    unsigned arg_blk_sub_gap_count = 0;

    bm::word_t* blk = sort_input_blocks_and(bv_src_and, src_and_size,
                                              i, j,
                                   &arg_blk_and_count, &arg_blk_and_gap_count);

    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);

    if (!blk) // nothing to do - golden block(!)
        return 0;
    
    digest_type digest = 0;

    if (arg_blk_and_count || arg_blk_and_gap_count)
    {
        // AND bit-blocks
        //
        digest = process_bit_blocks_and(arg_blk_and_count, digest);
        if (!digest)
            return digest;

        // AND all GAP blocks (if any)
        //
        if (arg_blk_and_gap_count)
        {
            digest =
                process_gap_blocks_and(arg_blk_and_gap_count, digest);
            if (!digest)
                return digest;
        }
    }
    else
    {
        return 0; // nothing to do
    }
    
    if (src_sub_size)
    {
        blk = sort_input_blocks_or(bv_src_sub, src_sub_size,
                                   i, j,
                                   &arg_blk_sub_count, &arg_blk_sub_gap_count);
        
        BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);
        if (blk == FULL_BLOCK_FAKE_ADDR)
            return 0; // nothing to do - golden block(!)
        
        if (arg_blk_sub_count || arg_blk_sub_gap_count)
        {
            digest = process_bit_blocks_sub(arg_blk_sub_count, digest);
            if (!digest)
                return digest;
            if (arg_blk_sub_gap_count)
            {
                digest =
                    process_gap_blocks_sub(arg_blk_sub_gap_count, digest);
            }
        }
    }
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::process_gap_blocks_or(blocks_manager_type& bman_target,
                                           unsigned i, unsigned j,
                                           unsigned arg_blk_gap_count)
{
    bm::word_t* blk = ar_->tb1;
    bool all_one;

    unsigned k = 0;
// experimental code, commented out as inefficient
#if 0
    unsigned unroll_factor = 4;
    unsigned len = arg_blk_gap_count - k;
    unsigned len_unr = len - (len % unroll_factor);
    
    const bm::gap_word_t* res;
    unsigned res_len;

    for (; k < len_unr; k+=unroll_factor)
    {
        {
            const bm::gap_word_t* gap1 = db_->v_arg_blk_gap[k];
            const bm::gap_word_t* gap2 = db_->v_arg_blk_gap[k+1];
            res = bm::gap_operation_or(gap1, gap2, db_->gap_res_buf1, res_len);
            BM_ASSERT(res == db_->gap_res_buf1);
            if (bm::gap_is_all_one(res, bm::gap_max_bits))
            {
                bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
                return true; // golden block found (all 1s)!
            }
        }
        {
            const bm::gap_word_t* gap1 = db_->v_arg_blk_gap[k+2];
            const bm::gap_word_t* gap2 = db_->v_arg_blk_gap[k+3];
            res = bm::gap_operation_or(gap1, gap2, db_->gap_res_buf2, res_len);
            BM_ASSERT(res == db_->gap_res_buf2);
            if (bm::gap_is_all_one(res, bm::gap_max_bits))
            {
                bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
                return true; // golden block found (all 1s)!
            }
        }
        
        res = bm::gap_operation_or(db_->gap_res_buf1, db_->gap_res_buf2, db_->gap_res_buf3, res_len);
        BM_ASSERT(res == db_->gap_res_buf3);
        if (bm::gap_is_all_one(res, bm::gap_max_bits))
        {
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true; // golden block found (all 1s)!
        }
        
        // accumulate the result of 4x gap OR
        bm::gap_add_to_bitset(blk, res);
    }
    unroll_factor = 2;
    len = arg_blk_gap_count - k;
    len_unr = len - (len % unroll_factor);

    for (; k < len_unr; k+=unroll_factor)
    {
        {
            const bm::gap_word_t* gap1 = db_->v_arg_blk_gap[k];
            const bm::gap_word_t* gap2 = db_->v_arg_blk_gap[k+1];
            res = bm::gap_operation_or(gap1, gap2, db_->gap_res_buf1, res_len);
            BM_ASSERT(res == db_->gap_res_buf1);
            if (bm::gap_is_all_one(res, bm::gap_max_bits))
            {
                bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
                return true; // golden block found (all 1s)!
            }
        }
        // accumulate the result of 2x gap OR
        bm::gap_add_to_bitset(blk, res);
    }
#endif

    for (; k < arg_blk_gap_count; ++k)
    {
        bm::gap_add_to_bitset(blk, ar_->v_arg_blk_gap[k]);
    }
    
    all_one = bm::is_bits_one((bm::wordop_t*) blk);
    if (all_one)
    {
        bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
        return true;
    }
    
    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_gap_blocks_and(unsigned   arg_blk_gap_count,
                                                   digest_type digest)
{
    BM_ASSERT(arg_blk_gap_count);
    BM_ASSERT(digest);

    bm::word_t* blk = ar_->tb1;
    
    unsigned k = 0;
    for (; k < arg_blk_gap_count; ++k)
    {
        bm::gap_and_to_bitset(blk, ar_->v_arg_blk_gap[k], digest);
        digest = bm::update_block_digest0(blk, digest);
        if (!digest)
        {
            BM_ASSERT(bm::bit_is_all_zero(blk));
            break;
        }
    }
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_gap_blocks_sub(unsigned   arg_blk_gap_count,
                                                   digest_type digest)
{
    BM_ASSERT(arg_blk_gap_count);
    BM_ASSERT(digest);

    bm::word_t* blk = ar_->tb1;
    
    unsigned k = 0;
    for (; k < arg_blk_gap_count; ++k)
    {
        bm::gap_sub_to_bitset(blk, ar_->v_arg_blk_gap[k], digest);
        digest = bm::update_block_digest0(blk, digest);
        if (!digest)
        {
            BM_ASSERT(bm::bit_is_all_zero(blk));
            break;
        }
    }
    return digest;
}


// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::process_bit_blocks_or(blocks_manager_type& bman_target,
                                           unsigned i, unsigned j,
                                           unsigned arg_blk_count)
{
    bm::word_t* blk = ar_->tb1;
    bool all_one;

    unsigned k = 0;

    if (arg_blk_count)  // copy the first block
        bm::bit_block_copy(blk, ar_->v_arg_blk[k++]);
    else
        bm::bit_block_set(blk, 0);

    // process all BIT blocks
    //
    unsigned unroll_factor, len, len_unr;
    
    unroll_factor = 4;
    len = arg_blk_count - k;
    len_unr = len - (len % unroll_factor);
    for( ;k < len_unr; k+=unroll_factor)
    {
        all_one = bm::bit_block_or_5way(blk,
                                        ar_->v_arg_blk[k], ar_->v_arg_blk[k+1],
                                        ar_->v_arg_blk[k+2], ar_->v_arg_blk[k+3]);
        if (all_one)
        {
            BM_ASSERT(blk == ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    unroll_factor = 2;
    len = arg_blk_count - k;
    len_unr = len - (len % unroll_factor);
    for( ;k < len_unr; k+=unroll_factor)
    {
        all_one = bm::bit_block_or_3way(blk, ar_->v_arg_blk[k], ar_->v_arg_blk[k+1]);
        if (all_one)
        {
            BM_ASSERT(blk == ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    for (; k < arg_blk_count; ++k)
    {
        all_one = bm::bit_block_or(blk, ar_->v_arg_blk[k]);
        if (all_one)
        {
            BM_ASSERT(blk == ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_bit_blocks_and(unsigned   arg_blk_count,
                                       digest_type digest)
{
    bm::word_t* blk = ar_->tb1;

    unsigned k = 0;
    
    switch (arg_blk_count)
    {
    case 0:
        bm::bit_block_set(ar_->tb1, ~0u); // set buffer to 0xFF...
        return ~0ull;
    case 1:
        bm::bit_block_copy(blk, ar_->v_arg_blk[k]);
        return bm::calc_block_digest0(blk);
    default:
        digest = bm::bit_block_and_2way(blk,
                                        ar_->v_arg_blk[k],
                                        ar_->v_arg_blk[k+1],
                                        ~0ull);
        k += 2;
        break;
    } // switch

    bit_decode_cache dcache;
    
    for (; k < arg_blk_count; ++k)
    {
        if (ar_->v_arg_blk[k] == FULL_BLOCK_REAL_ADDR)
            continue;
        digest = bm::bit_block_and(blk, ar_->v_arg_blk[k], digest, dcache);
        if (!digest) // all zero
            break;
    } // for k

    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_bit_blocks_sub(unsigned   arg_blk_count,
                                       digest_type digest)
{
    bm::word_t* blk = ar_->tb1;

    bit_decode_cache dcache;

    unsigned k = 0;
    for (; k < arg_blk_count; ++k)
    {
        if (ar_->v_arg_blk[k] == FULL_BLOCK_REAL_ADDR) // golden block
        {
            digest ^= digest;
            break;
        }
        bm::id64_t digest_n =
                    bm::bit_block_sub(blk, ar_->v_arg_blk[k], digest, dcache);
        BM_ASSERT(digest_n == bm::update_block_digest0(blk, digest));
        digest = digest_n;
        if (!digest) // all zero
            break;
    } // for k

    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned aggregator<BV>::resize_target(bvector_type& bv_target,
                            const bvector_type_const_ptr* bv_src, unsigned src_size,
                            bool init_clear)
{
    typename bvector_type::blocks_manager_type& bman_target = bv_target.get_blocks_manager();
    if (init_clear)
    {
        if (!bman_target.is_init())
            bman_target.init_tree();
        else
            bv_target.clear();
    }
    
    unsigned top_blocks = bman_target.top_block_size();
    auto size = bv_target.size();

    // pre-scan to do target size harmonization
    for (unsigned i = 0; i < src_size; ++i)
    {
        const bvector_type* bv = bv_src[i];
        BM_ASSERT(bv);
        const typename bvector_type::blocks_manager_type& bman_arg = bv->get_blocks_manager();
        unsigned arg_top_blocks = bman_arg.top_block_size();
        if (arg_top_blocks > top_blocks)
            top_blocks = bman_target.reserve_top_blocks(arg_top_blocks);
        auto arg_size = bv->size();
        if (arg_size > size)
        {
            bv_target.resize(arg_size);
            size = arg_size;
        }
    } // for i
    return top_blocks;
}

// ------------------------------------------------------------------------

template<typename BV>
bm::word_t* aggregator<BV>::sort_input_blocks_or(const bvector_type_const_ptr* bv_src,
                                                 unsigned src_size,
                                                 unsigned i, unsigned j,
                                                 unsigned* arg_blk_count,
                                                 unsigned* arg_blk_gap_count)
{
    bm::word_t* blk = 0;
    for (unsigned k = 0; k < src_size; ++k)
    {
        const bvector_type* bv = bv_src[k];
        BM_ASSERT(bv);
        const blocks_manager_type& bman_arg = bv->get_blocks_manager();
        const bm::word_t* arg_blk = bman_arg.get_block_ptr(i, j);
        if (!arg_blk)
            continue;
        if (BM_IS_GAP(arg_blk))
        {
            ar_->v_arg_blk_gap[*arg_blk_gap_count] = BMGAP_PTR(arg_blk);
            (*arg_blk_gap_count)++;
        }
        else // FULL or bit block
        {
            if (IS_FULL_BLOCK(arg_blk))
            {
                blk = FULL_BLOCK_FAKE_ADDR;
                *arg_blk_gap_count = *arg_blk_count = 0; // nothing to do
                break;
            }
            ar_->v_arg_blk[*arg_blk_count] = arg_blk;
            (*arg_blk_count)++;
        }
    } // for k
    return blk;
}

// ------------------------------------------------------------------------

template<typename BV>
bm::word_t* aggregator<BV>::sort_input_blocks_and(const bvector_type_const_ptr* bv_src,
                                                  unsigned src_size,
                                                  unsigned i, unsigned j,
                                                  unsigned* arg_blk_count,
                                                  unsigned* arg_blk_gap_count)
{
    bm::word_t* blk = FULL_BLOCK_FAKE_ADDR;
    for (unsigned k = 0; k < src_size; ++k)
    {
        const bvector_type* bv = bv_src[k];
        BM_ASSERT(bv);
        const typename bvector_type::blocks_manager_type& bman_arg = bv->get_blocks_manager();
        const bm::word_t* arg_blk = bman_arg.get_block_ptr(i, j);
        if (!arg_blk)
        {
            blk = 0;
            *arg_blk_gap_count = *arg_blk_count = 0; // nothing to do
            break;
        }
        if (BM_IS_GAP(arg_blk))
        {
            ar_->v_arg_blk_gap[*arg_blk_gap_count] = BMGAP_PTR(arg_blk);
            (*arg_blk_gap_count)++;
        }
        else // FULL or bit block
        {
            ar_->v_arg_blk[*arg_blk_count] =
                    IS_FULL_BLOCK(arg_blk) ? FULL_BLOCK_REAL_ADDR: arg_blk;
            (*arg_blk_count)++;
        }
    } // for k
    return blk;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or_horizontal(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, unsigned src_size)
{
    BM_ASSERT(src_size);
    if (src_size == 0)
    {
        bv_target.clear();
        return;
    }

    const bvector_type* bv = bv_src[0];
    bv_target = *bv;
    
    for (unsigned i = 1; i < src_size; ++i)
    {
        bv = bv_src[i];
        BM_ASSERT(bv);
        bv_target.bit_or(*bv);
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and_horizontal(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, unsigned src_size)
{
    BM_ASSERT(src_size);
    
    if (src_size == 0)
    {
        bv_target.clear();
        return;
    }
    const bvector_type* bv = bv_src[0];
    bv_target = *bv;
    
    for (unsigned i = 1; i < src_size; ++i)
    {
        bv = bv_src[i];
        BM_ASSERT(bv);
        bv_target.bit_and(*bv);
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and_sub_horizontal(bvector_type& bv_target,
                                                const bvector_type_const_ptr* bv_src_and,
                                                unsigned src_and_size,
                                                const bvector_type_const_ptr* bv_src_sub,
                                                unsigned src_sub_size)
{
    BM_ASSERT(src_and_size);

    combine_and_horizontal(bv_target, bv_src_and, src_and_size);

    for (unsigned i = 0; i < src_sub_size; ++i)
    {
        const bvector_type* bv = bv_src_sub[i];
        BM_ASSERT(bv);
        bv_target -= *bv;
    }
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::shift_right_and(bvector_type& bv_target,
                                     const bvector_type& bv_mask)
{
    unsigned any = 0;
    const blocks_manager_type& bman_arg = bv_mask.get_blocks_manager();

    if (!bman_arg.is_init()) // nothing to do
    {
        bv_target.clear();
        return any;
    }

    typename bvector_type::mem_pool_guard mp_guard;
    mp_guard.assign_if_not_set(pool_, bv_target); // set algorithm-local memory pool to avoid heap contention
    
    blocks_manager_type& bman_target = bv_target.get_blocks_manager();
    if (!bman_target.is_init())
        return 0;
    if (bv_target.size_ < bm::id_max)
        ++bv_target.size_;
    
    int block_type;
    bm::word_t carry_over = 0;
    
    unsigned top_blocks = bman_target.top_block_size();    
    bm::word_t*** blk_root = bman_target.top_blocks_root();
    bm::word_t** blk_blk;
    bm::word_t* block;

    for (unsigned i = 0; i < bm::set_array_size; ++i)
    {
        if (i >= top_blocks)
        {
            if (!carry_over)
                break;
            blk_blk = 0;
        }
        else
            blk_blk = blk_root[i];
        
        if (!blk_blk) // top level group of blocks missing - can skip it
        {
            if (carry_over)
            {
                unsigned nblock = (i * bm::set_array_size) + 0;
                const bm::word_t* arg_blk = bman_arg.get_block_ptr(i, 0);
                if (!arg_blk) // 1 & 0 == 0 - nothing to do
                {}
                else
                {
                    arg_blk = BLOCK_ADDR_SAN(arg_blk);
                    bm::word_t w0 = (carry_over & arg_blk[0]);
                    if (w0)
                    {
                        block =
                            bman_target.check_allocate_block(
                                         nblock, 0, 0, &block_type, false);
                        any = block[0] = w0;
                        
                        // reset all control vars (blocks tree may have re-allocated)
                        blk_root = bman_target.top_blocks_root();
                        blk_blk = blk_root[i];
                        top_blocks = bman_target.top_block_size();
                    }
                }
                carry_over = 0;
            }
            continue;
        }
        
        unsigned j = 0;
        do
        {
            unsigned nblock = (i * bm::set_array_size) + j;
            block = blk_blk[j];
            const bm::word_t* arg_blk = bman_arg.get_block(i, j);
            
            bm::word_t acc = 0;

            if (!block)
            {
                if (carry_over)
                {
                    bm::word_t w0 = 0;
                    if (arg_blk)
                    {
                        unsigned arg0 = BM_IS_GAP(arg_blk) ?
                                        bm::gap_test(BMGAP_PTR(arg_blk), 0)
                                        : arg_blk[0];
                        w0 = (carry_over & arg0);
                    }
                    if (w0)
                    {
                        block =
                            bman_target.check_allocate_block(
                                           nblock, 0, 0, &block_type, false);
                        blk_blk = blk_root[i];
                        any = block[0] = w0;
                    }
                    carry_over = 0;
                }
                // no CO - tight loop scan for the next available block
                // (optimizational only)
                {
                    for (++j; j < bm::set_array_size; ++j)
                    {
                        if (0 != (block = blk_blk[j]))
                            break;
                    } // for j
                    if (!block)
                        continue;
                    else
                    {
                        nblock = (i * bm::set_array_size) + j;
                        arg_blk = bman_arg.get_block(i, j);
                    }
                }
            }
            
            // check for various cases, when we can avoid de-optimization
            // and go with predicted result
            //
            if (BM_IS_GAP(block))
            {
                block = bman_target.deoptimize_block(nblock);
            }
            else
            if (IS_FULL_BLOCK(block)) // 0|1 into 11111 => (co==1)
            {
                if (carry_over) // 1 into 11111 => (co==1)
                {
                    if (IS_FULL_BLOCK(arg_blk)) // result is 1111, nothing to do
                        continue;
                }
                // 0|1 into 111111 => co==1
                if (!arg_blk) // result is zero, co==1
                {
                    bman_target.zero_block(nblock);
                    carry_over = 1;
                    continue;
                }
                
                block = bman_target.deoptimize_block(nblock);
            }

            if (BM_IS_GAP(arg_blk)) // GAP argument
            {
                carry_over = bm::bit_block_shift_r1_unr(block, &acc, carry_over);
                if (acc)
                {
                    bm::gap_and_to_bitset(block, BMGAP_PTR(arg_blk));
                    acc = !bm::bit_is_all_zero(block);
                }
            }
            else // 2 bit-blocks
            {
                if (arg_blk) // use fast fused SHIFT-AND operation
                {
                    if (FULL_BLOCK_REAL_ADDR == arg_blk) // AND is no-op (do SHIFT-R)
                        carry_over =
                        bm::bit_block_shift_r1_unr(block, &acc, carry_over);
                    else // do fused SHIFT-R-AND
                        carry_over =
                        bm::bit_block_shift_r1_and_unr(block, arg_blk,
                                                        &acc, carry_over);
                }
                else  // arg is zero - target block turns zero
                {
                    carry_over = block[bm::set_block_size-1] >> 31;
                    bman_target.zero_block(nblock);
                    block = 0; acc = 0;
                }
            }
            any |= acc;
            if (nblock == bm::set_total_blocks-1) // last possible block
            {
                carry_over = block[bm::set_block_size-1] & (1u<<31);
                block[bm::set_block_size-1] &= ~(1u<<31); // clear the 1-bit tail
                if (!acc) // block shifted out: release memory
                    bman_target.zero_block(nblock);
                break;
            }
            if (!acc && block)
            {
                BM_ASSERT(bm::bit_is_all_zero(block));
                bman_target.zero_block(nblock);
            }
            
        } while (++j < bm::set_array_size);
    } // for i

    return any;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_shift_right_and(
                bvector_type& bv_target,
                const bvector_type_const_ptr* bv_src_and, unsigned src_and_size,
                bool any)
{
    BM_ASSERT_THROW(src_size < max_aggregator_cap, BM_ERR_RANGE);
    if (!src_and_size)
    {
        bv_target.clear();
        return false;
    }
    unsigned top_blocks = resize_target(bv_target, bv_src_and, src_and_size);

    // set initial carry overs all to 0
    for (unsigned k = 0; k < src_and_size; ++k) // reset co flags
        ar_->carry_overs_[k] = 0;

    for (unsigned i = 0; i < bm::set_array_size; ++i)
    {
        if (i > top_blocks)
        {
            if (!this->any_carry_overs(src_and_size))
                break; // quit early if there is nothing to carry on
        }

        unsigned j = 0;
        do
        {
            bool found = combine_shift_right_and(i, j, bv_target, bv_src_and, src_and_size);
            if (found && any)
                return found;
        } while (++j < bm::set_array_size);

    } // for i

    return bv_target.any();
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_shift_right_and(unsigned i, unsigned j,
                                             bvector_type& bv_target,
                                        const bvector_type_const_ptr* bv_src,
                                        unsigned src_size)
{
    typename bvector_type::blocks_manager_type& bman_target =
                                                bv_target.get_blocks_manager();
    bm::word_t* blk = ar_->tb1;
    unsigned char* carry_overs = &(ar_->carry_overs_[0]);

    bm::word_t acc = 1; // by default currrent block has content

    // first initial block is just copied over from the first AND
    {
        const blocks_manager_type& bman_arg = bv_src[0]->get_blocks_manager();
        BM_ASSERT(bman_arg.is_init());
        const bm::word_t* arg_blk = bman_arg.get_block(i, j);
        if (BM_IS_GAP(arg_blk))
        {
            bm::bit_block_set(blk, 0);
            bm::gap_and_to_bitset(blk, BMGAP_PTR(arg_blk));
        }
        else
        {
            if (arg_blk)
                bm::bit_block_copy(blk, arg_blk);
            else
            {
                bm::bit_block_set(blk, 0);
                acc = 0;
            }
        }
        carry_overs[0] = 0;
    }
    
    for (unsigned k = 1; k < src_size; ++k)
    {
        unsigned carry_over = carry_overs[k];
        if (!acc && !carry_over) // 0 into "00000" block >> 0
        {
            BM_ASSERT(bm::bit_is_all_zero(blk));
            continue;
        }
        const blocks_manager_type& bman_arg = bv_src[k]->get_blocks_manager();
        const bm::word_t* arg_blk = bman_arg.get_block(i, j);
        
        if (BM_IS_GAP(arg_blk)) // GAP argument
        {
            carry_over = bm::bit_block_shift_r1_unr(blk, &acc, carry_over);
            if (acc)
            {
                bm::gap_and_to_bitset(blk, BMGAP_PTR(arg_blk));
                acc = !bm::bit_is_all_zero(blk);
            }
        }
        else // 2 bit-blocks
        {
            if (arg_blk) // use fast fused SHIFT-AND operation
            {
                if (FULL_BLOCK_REAL_ADDR == arg_blk) // AND is no-op (do SHIFT-R)
                    carry_over =
                        bm::bit_block_shift_r1_unr(blk, &acc, carry_over);
                else                                   // fused SHIFT-R-AND
                    carry_over =
                        bm::bit_block_shift_r1_and_unr(blk, arg_blk,
                                                       &acc, carry_over);
            }
            else  // arg is zero - target block => zero
            {
                unsigned co = blk[bm::set_block_size-1] >> 31; // carry out
                if (acc)
                    bm::bit_block_set(blk, 0);
                acc = blk[0] |= carry_over;
                carry_over = co;
            }
        }
        carry_overs[k] = bool(carry_over); // save new carry over
    } // for k
    
    // block now gets emitted into the target bit-vector
    if (acc)
    {
        BM_ASSERT(!bm::bit_is_all_zero(blk));
        unsigned nblock = (i * bm::set_array_size) + j;
        if (nblock == bm::set_total_blocks-1) // last possible block
            blk[bm::set_block_size-1] &= ~(1u<<31); // clear the 1-bit tail

        int block_type;
        bm::word_t* new_block =
            bman_target.check_allocate_block(
                              nblock, 0, 0, &block_type, false);
        bm::bit_block_copy(new_block, blk);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::any_carry_overs(unsigned co_size) const
{
    for (unsigned i = 0; i < co_size; ++i)
        if (ar_->carry_overs_[i])
            return true;
    return false;
}

// ------------------------------------------------------------------------


} // bm

#include "bmundef.h"

#endif
