#ifndef FUNCTION_H
#define FUNCTION_H

#include <memory>
#include <type_traits>

namespace my_function {

template <typename>
struct function;

template <typename T, typename... Args>
struct function<T(Args...)> {
private:
    static const size_t BUFFER_SIZE = 64;

    struct concept_ {
        virtual T call(Args &&... args) const = 0;
        virtual void copy_to_buffer(void *buffer) const = 0;
        virtual void move_to_buffer(void *buffer) noexcept = 0;
        virtual std::unique_ptr<concept_> copy() const = 0;
        virtual ~concept_() = default;
    };

    template <typename M>
    struct model : concept_ {
    private:
        M m;
    public:
        explicit model(M &&m): m(std::move(m)) {}
        explicit model(M const &m) : m(m) {}

        T call(Args &&... args) const override {
            return m(std::forward<Args>(args)...);
        }

        void copy_to_buffer(void *buffer) const override {
            new (buffer) model<M>(m);
        }

        void move_to_buffer(void *buffer) noexcept override {
            new (buffer) model<M>(std::move(m));
        }

        std::unique_ptr<concept_> copy() const override {
            return std::make_unique<model<M>>(m);
        }

        ~model() override = default;
    };

private:
    bool is_small;
    union {
        typename std::aligned_storage<BUFFER_SIZE, alignof(size_t)>::type buffer;
        std::unique_ptr<concept_> ptr;
    };

    void move_from(function &&other){
        is_small = other.is_small;
        if(is_small){
            reinterpret_cast<concept_ *>(&other.buffer)->move_to_buffer(&buffer);
            other = function();
        } else {
            new (&ptr) std::unique_ptr<concept_>(std::move(other.ptr));
        }
    }

public:
    function() noexcept : is_small(false), ptr(nullptr) {}

    function(std::nullptr_t) noexcept : is_small(false), ptr(nullptr) {}

    function(const function& other) noexcept {
        is_small = other.is_small;
        if(is_small) {
            reinterpret_cast<const concept_*>(&other.buffer)->copy_to_buffer(&buffer);
        } else {
            ptr = other.ptr->copy();
        }
    }

    function(function&& other) noexcept {
        move_from(std::move(other));
    }

    template<typename F>
    function(F f) {
        if (std::is_nothrow_move_constructible<F>::value && sizeof(model<F>) <= BUFFER_SIZE && alignof(model<F>) <= alignof(size_t)) {
            is_small = true;
            new (&buffer) model<F>(std::move(f));
        } else {
            is_small = false;
            new (&ptr) std::unique_ptr<concept_>(std::make_unique<model<F>>(std::move(f)));
        }
    }

    ~function() {
        if (is_small){
            (reinterpret_cast<concept_ *>(&buffer))->~concept_();
        } else {
            ptr.~unique_ptr();
        }
    }

    function& operator=(const function& other) {
        function t(other);
        swap(t);
        return *this;
    }

    function& operator=(function&& other) noexcept {
        if (is_small){
            (reinterpret_cast<concept_ *>(&buffer))->~concept_();
        } else {
            ptr.~unique_ptr();
        }
        move_from(std::move(other));
        return *this;
    }

    void swap(function& other) noexcept {
        function t(std::move(other));
        other = std::move(*this);
        *this = std::move(t);
    }

    explicit operator bool() const noexcept {
        return is_small || ptr;
    }

    T operator()(Args... args) const {
        if(is_small){
            return reinterpret_cast<concept_ const *>(&buffer)->call(std::forward<Args>(args)...);
        } else {
            return ptr->call(std::forward<Args>(args)...);
        }
    }
};

template <typename T, typename... Args>
void swap(function<T(Args...)> &lhs, function<T(Args...)> &rhs) noexcept {
    lhs.swap(rhs);
}

}

#endif // FUNCTION_H
