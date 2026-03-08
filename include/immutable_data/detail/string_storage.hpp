#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace data::detail
{

    template <std::size_t MaxSize>
    class string_storage
    {
    public:
        constexpr string_storage() = default;

        constexpr explicit string_storage(std::string_view str) noexcept
        {
            const auto copy_size = std::min(str.size(), MaxSize - 1);
            for (std::size_t i = 0; i < copy_size; ++i)
            {
                data_[i] = str[i];
            }
            size_ = copy_size;
            data_[size_] = '\0';
        }

        [[nodiscard]] constexpr auto view() const noexcept -> std::string_view
        {
            return {data_.data(), size_};
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return size_;
        }

        constexpr auto push_back(char c) noexcept -> bool
        {
            if (size_ < MaxSize - 1)
            {
                data_[size_++] = c;
                return true;
            }
            return false;
        }

        [[nodiscard]] constexpr auto full() const noexcept -> bool
        {
            return size_ >= MaxSize - 1;
        }

        [[nodiscard]] constexpr auto operator<=>(const string_storage &) const = default;

    private:
        std::array<char, MaxSize> data_{};
        std::size_t size_{0};
    };

} // namespace data::detail
