#ifndef MINECLONE_CLIENT_UTILITY_HPP_
#define MINECLONE_CLIENT_UTILITY_HPP_

#include "Common.hpp"

namespace MineClone
{

template <typename R, typename T, typename Predicate>
inline R FirstOrDefault(T &container, Predicate predicate, R defaultValue)
{
    auto iterator = std::find_if(begin(container), end(container), predicate);

    return iterator == end(container) ? defaultValue : *iterator;
}

} // namespace MineClone

#endif // MINECLONE_CLIENT_UTILITY_HPP_
