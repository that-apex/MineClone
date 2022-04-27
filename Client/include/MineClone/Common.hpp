#pragma once
#ifndef MINECLONE_CLIENT_COMMON_HPP_
#define MINECLONE_CLIENT_COMMON_HPP_

#include <cstdint>
#include <exception>
#include <string>

#define COPYABLE(type)                                                                                                 \
    type(const type &);                                                                                                \
    type &operator=(const type &)

#define NON_COPYABLE(type)                                                                                            \
    type(const type &) = delete;                                                                                       \
    type &operator=(const type &) = delete

#define MOVEABLE(type)                                                                                                 \
    type(type &&) noexcept;                                                                                            \
    type &operator=(type &&) noexcept

#define NON_MOVABLE(type)                                                                                              \
    type(type &&) noexcept = delete;                                                                                   \
    type &operator=(type &&) noexcept = delete

#define ASSERT(condition, message)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            throw MineClone::Exception(message);                                                                       \
        }                                                                                                              \
    } while (0)

namespace MineClone
{

using namespace std::literals::string_literals;

class Exception : public std::exception
{
  public:
    inline explicit Exception(std::string message, int errorCode = 1) noexcept
        : m_message(std::move(message)), m_errorCode(errorCode)
    {
    }

    [[nodiscard]] inline const char *what() const noexcept override
    {
        return m_message.c_str();
    }

    [[nodiscard]] inline int ErrorCode() const noexcept
    {
        return m_errorCode;
    }

  private:
    const std::string m_message;
    const int m_errorCode;
}; // class Exception

} // namespace MineClone

#endif // MINECLONE_CLIENT_COMMON_HPP_