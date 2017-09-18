/*
 * Copyright (C) 2015, Simon Fuhrmann
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#ifndef MATH_ALGO_HEADER
#define MATH_ALGO_HEADER

#include <cmath>
#include <vector>
#include <iterator>
#include <stdexcept>

#include "math/defines.h"

MATH_NAMESPACE_BEGIN
MATH_ALGO_NAMESPACE_BEGIN

/* ---------------------------- Algorithms ------------------------ */

/**
 * Algorithm that finds the value corresponding to a key in sorted vector
 * of key-value pairs. If the key does not exist, null is returned.
 */
template <typename Key, typename Value>
Value const*
binary_search (std::vector<std::pair<Key, Value> > const& vec, Key const& key)
{
    std::size_t range1 = 0;
    std::size_t range2 = vec.size();
    while (range1 != range2)
    {
        std::size_t pos = (range1 + range2) / 2;
        if (key < vec[pos].first)
            range2 = pos;
        else if (vec[pos].first < key)
            range1 = pos + 1;
        else
            return &vec[pos].second;
    }

    return nullptr;
}

/* ------------------- Misc: predicates, iterators, ... ----------- */

/** Squared sum accumulator. */
template <typename T>
inline T
accum_squared_sum (T const& init, T const& next)
{
    return init + next * next;
}

/** Absolute sum accumulator. */
template <typename T>
inline T
accum_absolute_sum (T const& init, T const& next)
{
    return init + std::abs(next);
}

/** Epsilon comparator predicate. */
template <typename T>
struct predicate_epsilon_equal
{
    T eps;
    explicit predicate_epsilon_equal (T const& eps) : eps(eps) {}
    bool operator() (T const& v1, T const& v2)
    { return (v1 - eps <= v2 && v2 <= v1 + eps); }
};

/** Iterator that advances 'S' elements of type T. */
template <typename T, int S>
struct InterleavedIter : public std::iterator<std::input_iterator_tag, T>
{
    T const* pos;
    explicit InterleavedIter (T const* pos) : pos(pos) {}
    InterleavedIter& operator++ (void) { pos += S; return *this; }
    T const& operator* (void) const { return *pos; }
    bool operator== (InterleavedIter<T,S> const& other) const
    { return pos == other.pos; }
    bool operator!= (InterleavedIter<T,S> const& other) const
    { return pos != other.pos; }
};

/* --------------------------- Vector tools ----------------------- */

/**
 * Erases all elements from 'vector' that are marked with 'true'
 * in 'delete_list'. The remaining elements are kept in order but relocated
 * to another position in the vector.
 */
template <typename T>
void
vector_clean (std::vector<bool> const& delete_list, std::vector<T>* vector)
{
    typename std::vector<T>::iterator vr = vector->begin();
    typename std::vector<T>::iterator vw = vector->begin();
    typename std::vector<bool>::const_iterator dr = delete_list.begin();

    while (vr != vector->end() && dr != delete_list.end())
    {
        if (*dr++)
        {
            vr++;
            continue;
        }
        if (vw != vr)
            *vw = *vr;
        vw++;
        vr++;
    }
    vector->erase(vw, vector->end());
}

/* ------------------------------ Misc ---------------------------- */

template <typename T>
inline void
sort_values (T* a, T* b, T* c)
{
    if (*b < *a)
        std::swap(*a, *b);
    if (*c < *b)
        std::swap(*b, *c);
    if (*b < *a)
        std::swap(*b, *a);
}

/* ------------------------ for-each functors --------------------- */

/** for-each functor: multiplies operand with constant factor. */
template <typename T>
struct foreach_multiply_with_const
{
    T value;
    explicit foreach_multiply_with_const (T const& value) : value(value) {}
    void operator() (T& val) { val *= value; }
};

/** for-each functor: divides operand by constant divisor. */
template <typename T>
struct foreach_divide_by_const
{
    T div;
    explicit foreach_divide_by_const (T const& div) : div(div) {}
    void operator() (T& val) { val /= div; }
};

/** for-each functor: adds a constant value to operand. */
template <typename T>
struct foreach_addition_with_const
{
    T value;
    explicit foreach_addition_with_const (T const& value) : value(value) {}
    void operator() (T& val) { val += value; }
};

/** for-each functor: substracts a constant value to operand. */
template <typename T>
struct foreach_substraction_with_const
{
    T value;
    explicit foreach_substraction_with_const (T const& value) : value(value) {}
    void operator() (T& val) { val -= value; }
};

/** for-each functor: raises each operand to the power of constant value. */
template <typename T>
struct foreach_constant_power
{
    T value;
    explicit foreach_constant_power (T const& value) : value(value) {}
    void operator() (T& val) { val = std::pow(val, value); }
};

/** for-each functor: matrix-vector multiplication. */
template <typename M, typename V>
struct foreach_matrix_mult
{
    M mat;
    explicit foreach_matrix_mult (M const& matrix) : mat(matrix) {}
    void operator() (V& vec) { vec = mat * vec; }
};

/** for-each functor: applies absolute value to operand. */
template <typename T>
inline void
foreach_absolute_value (T& val)
{
    val = std::abs(val);
}

/** for-each functor: negates the operand. */
template <typename T>
inline void
foreach_negate_value (T& val)
{
    val = -val;
}

MATH_ALGO_NAMESPACE_END
MATH_NAMESPACE_END

#endif /* MATH_ALGO_HEADER */
