#pragma once

#include <utility>

namespace netcore::detail {
    template <typename T>
    class list {
        list* head = this;
        list* tail = this;
    public:
        list() = default;

        list(const list&) = delete;

        list(list&& other) noexcept :
            head(std::exchange(other.head, std::addressof(other))),
            tail(std::exchange(other.tail, std::addressof(other)))
        {}

        ~list() {
            unlink();
        }

        auto operator=(const list&) -> list& = delete;

        auto operator=(list&& other) noexcept -> list& {
            head = std::exchange(other.head, std::addressof(other));
            tail = std::exchange(other.tail, std::addressof(other));

            head->tail = this;
            tail->head = this;
        }

        auto empty() const noexcept -> bool {
            return head == this;
        }

        template <typename F>
        requires requires(F f, T& t) { { f(t) }; }
        auto for_each(F&& f) -> void {
            auto* current = this;

            do {
                f(current->get());
                current = current->head;
            } while (current != this);
        }

        auto get() noexcept -> T& {
            return *static_cast<T*>(this);
        }

        auto link(list& other) noexcept -> void {
            other.head = this;
            other.tail = tail;

            tail->head = &other;
            tail = &other;
        }

        auto size() const noexcept -> std::size_t {
            auto* current = head;
            std::size_t result = 1;

            while (current != this) {
                ++result;
                current = current->head;
            }

            return result;
        }

        auto unlink() noexcept -> void {
            head->tail = tail;
            tail->head = head;

            head = this;
            tail = this;
        }
    };
}
