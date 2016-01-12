// -*- c++ -*-
#if !defined(JASNAH_H)
/* ==========================================================================
   $File: Jasnah.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015-2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
/* ==========================================================================
   CHANGELOG:
    - Update 0.4.1: Minor bugfixes
    - Update 0.4.0: Stabilized API
    - Update 0.3.0: Currying and Piping
    - Initial release 0.2.0: Fully featured Option type
   ========================================================================== */
/* ==========================================================================
   TODO:
     - RTTI typename info
     - Clojure style transducer implementation
   ========================================================================== */
/* ==========================================================================
   Using code from:
     - IndexSeq roughly as per C++14 standard
       Based on github.com/taocpp/sequences from Daniel Frey under MIT
       license

     - Currying and piping based on vitiy.info from Victor Laskin
       under granted permission (comment on Blog)

     - http://stackoverflow.com/a/5365786/3847013 for struct
       unpacking, under SO CC-BY license from James McNellis

   ==========================================================================
   Contained herein:

   - Everything callable is in namespace Jasnah (other than the preprocessor
     stuff)

   - Option<T>: behaves similarly to Rust's Option and C++17's
     std::optional, holding either some value of T, or Jasnah::None

   - MakeOption(T&& v): used to make an Option<T> from v

   - IsOption<T>: use IsOption<T>::value (type bool) to detect if T is
     an Option type

   - MakeIndexSeq<Length>: similar to C++14 standard, returns
     Jasnah::Impl::IndexSeq<std::size_t, 0, 1,...,Length-1>

   - UnwrapTupleIntoFn: used to call a function with the parameters
     contained in a std::tuple as its arguments, primarily a function
     used for the implementation here, but may be useful

   - Curry: a struct that can be used to curry function arguments and
     be used in piped chains (think UNIX). See below on usage of these
     operators

   - MakeCurry: Used to easily make curry objects from a lambda or
     function, currently not non-static member functions or functors
     -- wrap these in a lambda for now.
     Use JAS_TEMPLATE_FN(Name, Fn) to wrap a templated function.

   - Container Library : Basic implementations of Reduce(foldl), Map
     and Filter, from functional programming. Overloads to reserve
     std::vector where applicable. TODO: foldr MutableMap

   - JasUnpack : some macro trickery to bind references to data in
     something like a struct to the current scope. Call with object to
     unpack from, followed by names of members to unpack (up to 5
     members)

   ==========================================================================
   Currying:

   For a Curry object f

   1 >> f sets the first argument to 1
   2 >> 1 >> f sets the first argument to 2 and the second to 1
   f << 1 sets the last argument to 1
   f << 1 << 2 sets the second to last argument to 1 and the last to 1

   1 | f << 2 sets the last argument to 2 and pipes 1 into the first
   empty slot of f

   When piping an Option<T> (unless disabled), it is deref'd to its
   internal type for the next function, if it contains a value. If the
   function it's piped into returns an Option<U> and Option<T> ==
   None, then the Option<U> is automatically set to None. If a None is
   propagated into a non-option type, then an exception or abort is
   thrown, depending on the settings.

   ==========================================================================
   Available Preprocessor configuration switches:
   #define these to enable them

   - JASNAH_NO_EXCEPTIONS : Disables use of exceptions in the piping,
     if something goes wrong in this mode (e.g. attemting to pipe
     Jasnah::None into a non Option) the program will print an error
     and abort. Otherwise it will throw a std::invalid_argument

   - JASNAH_NO_OPTION_SPEC : Define this to make Jasnah not treat
     Option<T>'s differently to any other type (i.e. no longer
     checking and derefercing them on function call)

   - JASNAH_ASSERT : Define this to make Jasnah use your own assert
     function, by default the assert from the C standard library is
     used

   - JAS_LOG
   - JAS_LOG_END : Define these to make Jasnah use your own logging
     functions, you must make sure these are included before Jasnah is
     and these must be useable so that JAS_LOG "str" JAS_LOG_END;
     outputs "str" to your log. If undefined then the builtin
     implementation is used (stderr)

   - JASNAH_CPP_LOG : Use builtin logging functions, but C++ iostream
     rather than cstdio, invalid if you have defined JAS_LOG and
     JAS_LOG_END

   - JASNAH_NO_CTR_LIB : Define this to disable the builtin container
     manipulation functions such as Map, Filter and Reduce

   - JASNAH_NO_UNPACK : Define this to disable struct unpacking with
     JasUnpack
   ========================================================================== */

#define JASNAH_H
#include <type_traits>
#include <functional>
#include <tuple>


#ifndef JASNAH_NO_EXCEPTIONS
#include <stdexcept>
#else
#include <cstdlib>
#endif

#ifndef JASNAH_ASSERT
    #include <cassert>
    #define JASNAH_ASSERT(x) assert(x)
#endif

#define JASNAH_CPP_LOG

#ifndef JAS_LOG
    #ifndef JASNAH_CPP_LOG // Logs in red to stderr
        #include <cstdio>
        #define JAS_LOG fputs("\x1b[31m"
        #define JAS_LOG_END "\x1b[0m", stderr)
    #else
        #include <iostream>
        #define JAS_LOG std::cerr << "\x1b[31m"
        #define JAS_LOG_END "\x1b[0m"
    #endif
#endif

#define JAS_TEMPLATE_FN_IMPL2(NewObj, Fn, Line) struct CallImpl##Fn##Line  \
{                                                       \
    template <typename... Args>                         \
    constexpr auto                                      \
    operator()(Args&&... args) const                    \
        -> decltype(Fn(std::forward<Args>(args)...))    \
    {                                                   \
        return Fn(std::forward<Args>(args)...);         \
    }                                                   \
};                                                      \
const auto NewObj = Jasnah::MakeCurry(CallImpl##Fn##Line())
#define JAS_TEMPLATE_FN_IMPL1(NewObj, Fn, Line) JAS_TEMPLATE_FN_IMPL2(NewObj, Fn, Line)
#define JAS_TEMPLATE_FN(Name, Fn) JAS_TEMPLATE_FN_IMPL1(Name, Fn, __LINE__)


namespace Jasnah
{
    // Typenames for specialised Option<T> constructors
    struct InPlace_Internal {};
    constexpr InPlace_Internal ConstructInPlace{};

    /// Used to construct empty Option's and for comparing them against
    struct EmptyOption_Internal {};
    constexpr EmptyOption_Internal None{};

/* ==========================================================================
   Main Option class - based on not yet standard std::optional<T> proposed for C++17
   A couple of differences:
     - No exceptions
     - As a consequence of the above, no Value() method throwing an exception if empty
     - We use the Rust lang syntax of Jasnah::None for an empty
       Option<T> and .Some() if the object contains a value
   ========================================================================== */
    template <class T>
    class Option
    {
    private:
        /// Internal storage implementation for lazy type construction
        struct OptionStorage
        {
            // store as union of smallest integral type and T, the
            // standard mandates that the first named member be 0
            // initialized, i.e. the char, thus this can be far faster
            // than a costly T construction
            union
            {
                /// Object in use when empty
                char none;
                /// Object in use when some value is stored
                T value;
            };
            /// Flag indicating whether or not a value is stored
            bool some = false;

            /// Destruct T if in use
            ~OptionStorage()
            {
                if (some)
                    value.~T();
            }

            /// Construct empty by default
            constexpr OptionStorage()
                : none(0)
            {}

            /// Copy and move constructors using placement new
            OptionStorage(const OptionStorage& other)
                : some(other.some)
            {
                if (some)
                    ::new(std::addressof(value)) T(other.value);
            }

            OptionStorage(OptionStorage&& other)
                : some(other.some)
            {
                if (some)
                    ::new(std::addressof(value)) T(std::move(other.value));
            }

            /// Compile time constexpr constructors
            constexpr OptionStorage(const T& val)
                : value(val),
                  some(true)
            {}

            constexpr OptionStorage(T&& val)
                : value(std::move(val)),
                  some(true)
            {}

            /// Construct inplace with the magic of perfect forwarding
            template <class... Args>
            constexpr explicit
            OptionStorage(InPlace_Internal, Args&&... args)
                : value(std::forward<Args>(args)...),
                  some(true)
            {}
        };

        /// Declare an instance of the storage class for use in Option
        OptionStorage storage_;

    public:
        // Standard compliance typedef
        typedef T value_type;

        // Check it is valid to use Option with T
        static_assert(!std::is_reference<T>::value,
                      "Option<T> cannot be instantiated with a reference type");

        static_assert(std::is_object<T>::value,
                      "Option<T> must be instantiated with an object type (i.e. must have storage)");

        // Construct with None
        constexpr Option()
            : storage_()
        {}
        constexpr Option(EmptyOption_Internal)
            : storage_()
        {}

        // Construct from a value of T
        constexpr Option(const T& val)
            : storage_(val)
        {}

        constexpr Option(T&& val)
            : storage_(std::move(val))
        {}

        // Copy and move constructors
        Option(const Option&) = default;
        Option(Option&&) = default;
        // Destructor
        ~Option() = default;

        // SFINAE
        /// Construct inplace via perfect forwarding
        template <class... Args,
                  class = typename std::enable_if<
                      std::is_constructible<T, Args&&...>::value>::type
                  >
        constexpr explicit
        Option(InPlace_Internal, Args&&... args)
            : storage_(ConstructInPlace, std::forward<Args>(args)...)
        {}

        // SFINAE
        /// Construct inplace from initializer_list via perfect forwarding
        template <class U, class... Args,
                  class = typename std::enable_if<
                      std::is_constructible<T, std::initializer_list<U>&, Args&&...>::value>::type
                  >
        constexpr explicit
        Option(InPlace_Internal, std::initializer_list<U> ilist, Args&&... args)
            : storage_(ConstructInPlace, ilist, std::forward<Args>(args)...)
        {}

        /// Assignment operator to None
        Option&
        operator=(EmptyOption_Internal)
        {
            if (storage_.some)
            {
                storage_.value.~T();
                storage_.some = false;
            }
            return *this;
        }

        /// Assignment operator
        Option&
        operator=(const Option& other)
        {
            // If we have values, assign them
            if (storage_.some && other.storage_.some)
            {
                storage_.value = other.storage_.value;
            }
            else if (storage_.some)
            {
                // If this has a value (but the other can't because of
                // above) then set this to None
                storage_.value.~T();
                storage_.some = false;
            }
            else if (other.storage_.some)
            {
                // If we have no value but the other does then assign it
                ::new(&(storage_.value)) T(other.storage_.value);
                storage_.some = true;
            }
            return *this;
        }

        /// Move assignment operator, see above
        Option&
        operator=(Option&& other)
        {
            if (storage_.some && other.storage_.some)
            {
                storage_.value = std::move(other.storage_.value);
            }
            else if (storage_.some)
            {
                storage_.value.~T();
                storage_.some = false;
            }
            else if (other.storage_.some)
            {
                ::new(&(storage_.value)) T(std::move(other.storage_.value));
                storage_.some = true;
            }
            return *this;
        }

        /// Assign from another type U iff U can decay to T
        template <class U,
                  class = typename std::enable_if<
                      std::is_same<typename std::decay<U>::type, T>::value
                      >::type>
        Option&
        operator=(U&& val)
        {
            if (storage_.some)
                storage_.value = std::forward<U>(val);
            else
            {
                ::new(&(storage_.value)) T(std::forward<U>(val));
                storage_.some = true;
            }
            return *this;
        }

        /// Bool conversion returns whether the Option contains a value
        constexpr explicit
        operator bool() const
        {
            return storage_.some;
        }

        /// Returns whether the Option does NOT contain a value, sometimes clearer
        inline constexpr
        bool
        IsNone() const
        {
            return !(storage_.some);
        }

        /// Return the value stored, or if there isn't one, the value provided to the function
        template <class U>
        constexpr T
        ValueOr(U&& defaultVal) const& // to be used when this object is an lvalue
        {
            static_assert(std::is_copy_constructible<T>::value,
                          "T must be copy constructible to use Option<T>::ValueOr");
            static_assert(std::is_convertible<U, T>::value,
                          "U must be convertible to T to use Option<T>::ValueOr");

            return (storage_.some) ? storage_.value :
                static_cast<T>(std::forward<U>(defaultVal));
        }

        /// Return the value stored, or if there isn't one, the value provided to the function
        template <class U>
        T
        ValueOr(U&& defaultVal) && // Used when this object is an rvalue
        {
            static_assert(std::is_move_constructible<T>::value,
                          "T must be move constructible to use Option<T>::ValueOr");
            static_assert(std::is_convertible<U, T>::value,
                          "U must be convertible to T to use Option<T>::ValueOr");
            return (storage_.some) ? std::move(storage_.value) :
                static_cast<T>(std::forward<U>(defaultVal));
        }

        /// Fill with value constructed inplace
        template <class... Args,
                  class = typename std::enable_if<
                      std::is_constructible<T, Args&&...>::value>::type
                  >
        void
        Emplace(Args&&... args)
        {
            *this = None;
            ::new(&(storage_.value)) T(std::forward<Args>(args)...);
            storage_.some = true;
        }

        /// Fill with value constructed inplace from an initializer_list
        template <class U, class... Args,
                  class = typename std::enable_if<
                      std::is_constructible<T, std::initializer_list<U>&, Args&&...>::value>::type
                  >
        void
        Emplace(std::initializer_list<U> ilist, Args&&... args)
        {
            *this = None;
            ::new(&(storage_.value)) T(ilist, std::forward<Args>(args)...);
            storage_.some = true;
        }

        /// Access members via overloaded ->
        constexpr const T*
        operator->() const
        {
            // JASNAH_ASSERT(storage_.some && "Option dereferenced (->) for None");
            return (JASNAH_ASSERT(storage_.some && "Option dereferenced (->) for None"),
                    std::addressof(storage_.value));
        }

        /// Access members via overloaded ->
        inline T*
        operator->()
        {
            JASNAH_ASSERT(storage_.some && "Option dereferenced (->) for None");
            return &(storage_.value);
        }

        /// Access members via overloaded *
        constexpr const T&
        operator*() const
        {
            return (JASNAH_ASSERT(storage_.some && "Option dereferenced (->) for None"),
                    storage_.value);
        }

        /// Access members via overloaded *
        T&
        operator*()
        {
            JASNAH_ASSERT(storage_.some && "Option dereferenced (*) for None");
            return storage_.value;
        }
    };

    /// Helper function to construct Option with forwarding
    template <class T>
    inline constexpr
    Option<typename std::decay<T>::type>
    MakeOption(T&& val)
    {
        return Option<typename std::decay<T>::type>(std::forward<T>(val));
    }

    /// Template for detecting if T is a Jasnah::Option
    template <typename T>
    struct IsOption
    {
        static constexpr bool value = false;
    };

    template <typename T>
    struct IsOption<Jasnah::Option<T> >
    {
        static constexpr bool value = true;
    };

    /// All the comparison operators. Implemented as per the proposed standard
    template <class T>
    inline constexpr
    bool
    operator==(const Option<T>& lhs, const Option<T>& rhs)
    {
        return (static_cast<bool>(lhs) != static_cast<bool>(rhs))
            ? false
            : ((!static_cast<bool>(rhs))
               ? true
               : (*lhs == *rhs));
    }

    template <class T>
    inline constexpr
    bool
    operator!=(const Option<T>& lhs, const Option<T>& rhs)
    {
        return !(lhs == rhs);
    }

    template <class T>
    inline constexpr
    bool
    operator<(const Option<T>& lhs, const Option<T>& rhs)
    {
        return (!static_cast<bool>(rhs))
            ? false
            : (!(static_cast<bool>(lhs))
               ? true
               : (*lhs < *rhs));
    }

    template <class T>
    inline constexpr
    bool
    operator<=(const Option<T>& lhs, const Option<T>& rhs)
    {
        return !(rhs < lhs);
    }

    template <class T>
    inline constexpr
    bool
    operator>(const Option<T>& lhs, const Option<T>& rhs)
    {
        return rhs < lhs;
    }

    template <class T>
    inline constexpr
    bool
    operator>=(const Option<T>& lhs, const Option<T>& rhs)
    {
        return !(lhs < rhs);
    }

    /// Compare with EmptyOption for sorting etc.
    template <class T>
    inline constexpr
    bool
    operator==(const Option<T>& x, EmptyOption_Internal)
    {
        return !(static_cast<bool>(x));
    }

    template <class T>
    inline constexpr
    bool
    operator==(EmptyOption_Internal, const Option<T>& x)
    {
        return !(static_cast<bool>(x));
    }

    template <class T>
    inline constexpr
    bool
    operator!=(const Option<T>& x, EmptyOption_Internal)
    {
        return static_cast<bool>(x);
    }

    template <class T>
    inline constexpr
    bool
    operator!=(EmptyOption_Internal, const Option<T>& x)
    {
        return static_cast<bool>(x);
    }

    template <class T>
    inline constexpr
    bool
    operator<(const Option<T>& x, EmptyOption_Internal)
    {
        return false;
    }

    template <class T>
    inline constexpr
    bool
    operator<(EmptyOption_Internal, const Option<T>& x)
    {
        return static_cast<bool>(x);
    }

    template <class T>
    inline constexpr
    bool
    operator<=(const Option<T>& x, EmptyOption_Internal)
    {
        return !(static_cast<bool>(x));
    }

    template <class T>
    inline constexpr
    bool
    operator<=(EmptyOption_Internal, const Option<T>& x)
    {
        return true;
    }

    template <class T>
    inline constexpr
    bool
    operator>(const Option<T>& x, EmptyOption_Internal)
    {
        return static_cast<bool>(x);
    }

    template <class T>
    inline constexpr
    bool
    operator>(EmptyOption_Internal, const Option<T>& x)
    {
        return false;
    }

    template <class T>
    inline constexpr
    bool
    operator>=(const Option<T>& x, EmptyOption_Internal)
    {
        return true;
    }

    template <class T>
    inline constexpr
    bool
    operator>=(EmptyOption_Internal, const Option<T>& x)
    {
        return !(static_cast<bool>(x));
    }

    /// Compare Option<T> with a T
    template <class T>
    inline constexpr
    bool
    operator==(const Option<T>& x, const T& val)
    {
        return (static_cast<bool>(x))
            ? (*x == val)
            : false;
    }

    template <class T>
    inline constexpr
    bool
    operator==(const T& val, const Option<T>& x)
    {
        return (static_cast<bool>(x))
            ? (val == *x)
            : false;
    }

    template <class T>
    inline constexpr
    bool
    operator!=(const Option<T>& x, const T& val)
    {
        return (static_cast<bool>(x))
            ? !(*x == val)
            : true;
    }

    template <class T>
    inline constexpr
    bool
    operator!=(const T& val, const Option<T>& x)
    {
        return (static_cast<bool>(x))
            ? !(val == *x)
            : true;
    }

    template <class T>
    inline constexpr
    bool
    operator<(const Option<T>& x, const T& val)
    {
        return (static_cast<bool>(x))
            ? (*x < val)
            : true;
    }

    template <class T>
    inline constexpr
    bool
    operator<(const T& val, const Option<T>& x)
    {
        return (static_cast<bool>(x))
            ? (val < *x)
            : false;
    }

    template <class T>
    inline constexpr
    bool
    operator>(const Option<T>& x, const T& val)
    {
        return (static_cast<bool>(x))
            ? (val < *x)
            : false;
    }

    template <class T>
    inline constexpr
    bool
    operator>(const T& val, const Option<T>& x)
    {
        return (static_cast<bool>(x))
            ? (*x < val)
            : true;
    }

    template <class T>
    inline constexpr
    bool
    operator<=(const Option<T>& x, const T& val)
    {
        return !(x > val);
    }

    template <class T>
    inline constexpr
    bool
    operator<=(const T& val, const Option<T>& x)
    {
        return !(val > x);
    }

    template <class T>
    inline constexpr
    bool
    operator>=(const Option<T>& x, const T& val)
    {
        return !(x < val);
    }

    template <class T>
    inline constexpr
    bool
    operator>=(const T& val, const Option<T>& x)
    {
        return !(val < x);
    }

/* ==========================================================================
   Implemenation details of IndexSeq and UnwrapTupleIntoFn
   ========================================================================== */
    namespace Impl
    {
        template <typename T, T...  Indices>
        struct IndexSeq
        {
            typedef T value_type;

            static constexpr
            std::size_t
            Size()
            {
                return sizeof...(Indices);
            }
        };

        template <typename, std::size_t, bool Even>
        struct DoubleLength;

        template <typename T, T... Indices, std::size_t Length>
        struct DoubleLength<IndexSeq<T, Indices...>, Length, false> // even
        {
            using type = IndexSeq<T, Indices..., (Length + Indices)...>;
        };

        template <typename T, T... Indices, std::size_t Length>
        struct DoubleLength<IndexSeq<T, Indices...>, Length, true> // odd
        {
            using type = IndexSeq<T, Indices..., (Length + Indices)..., 2*Length>;
        };

        template <std::size_t Length, typename = void>
        struct MakeIndexSeqImpl;

        template <std::size_t Length>
        struct MakeIndexSeqImpl<Length, typename std::enable_if<Length==0>::type>
        {
            using type = IndexSeq<std::size_t>;
        };

        template <std::size_t Length>
        struct MakeIndexSeqImpl<Length, typename std::enable_if<Length==1>::type>
        {
            using type = IndexSeq<std::size_t, 0>;
        };

        template <std::size_t Length, typename>
        struct MakeIndexSeqImpl
            : DoubleLength<typename MakeIndexSeqImpl<Length/2>::type, Length/2, Length%2 == 1>
        {};

        template <typename Func, typename... Args, std::size_t... Seq>
        inline auto
        UnwrapTupleIntoFn(IndexSeq<std::size_t, Seq...>,
                          const Func& f,
                          const std::tuple<Args...>& fnArgs)
            -> decltype(f(std::get<Seq>(fnArgs)...))
        {
            return f(std::get<Seq>(fnArgs)...);
        }
    }

/* ==========================================================================
   Definitions of MakeIndexSeq and UnwrapTupleIntoFn
   ========================================================================== */
    template <std::size_t Length>
    using MakeIndexSeq = Impl::MakeIndexSeqImpl<Length>;

    template <typename Func, typename... Args>
    inline auto
    UnwrapTupleIntoFn(const Func& f, const std::tuple<Args...>& fnArgs)
        -> decltype(f(std::declval<Args>()...))
    {
        return Impl::UnwrapTupleIntoFn(typename MakeIndexSeq<sizeof...(Args)>::type(), f, fnArgs);
    }

/* ==========================================================================
   Definition of Curry, best called using MakeCurry
   ========================================================================== */
    template <class Func, typename LeftArgs = std::tuple<>, typename RightArgs = std::tuple<> >
    struct Curry
    {
    private:
        Func f;
        LeftArgs left;
        RightArgs right;
    public:

        Curry(Func&& f)
            : f(std::forward<Func>(f)),
              left(std::tuple<>()),
              right(std::tuple<>())
        {}

        Curry(const Func& f, const LeftArgs& l, const RightArgs& r)
            : f(f),
              left(l),
              right(r)
        {}

        template <typename... Args>
        auto
        operator()(Args&&... fnArgs) const
            -> decltype(UnwrapTupleIntoFn(f, std::tuple_cat(left, std::make_tuple(fnArgs...), right)))
        {
            return UnwrapTupleIntoFn(f, std::tuple_cat(left,
                                                       std::make_tuple(std::forward<Args>(fnArgs)...),
                                                       right));
        }

        template <typename T>
        auto
        LeftCurry(T&& fnArg) const
            -> decltype(Curry<Func, decltype(std::tuple_cat(left, std::make_tuple(fnArg))), RightArgs>
                        (f, std::tuple_cat(left, std::make_tuple(fnArg)), right))
        {
            return Curry<Func, decltype(std::tuple_cat(left, std::make_tuple(fnArg))), RightArgs>
                (f, std::tuple_cat(left, std::make_tuple(fnArg)), right);
        }

        template <typename T>
        auto
        RightCurry(T&& fnArg) const
            -> decltype(Curry<Func, LeftArgs, decltype(std::tuple_cat(right, std::make_tuple(fnArg)))>
                        (f, left, std::tuple_cat(right, std::make_tuple(fnArg))))
        {
            return Curry<Func, LeftArgs, decltype(std::tuple_cat(right, std::make_tuple(fnArg)))>
                (f, left, std::tuple_cat(right, std::make_tuple(fnArg)));
        }
    };


    template <typename Func>
    auto
    MakeCurry(Func&& f)
        -> decltype(Curry<Func>(std::forward<Func>(f)))
    {
        return Curry<Func>(std::forward<Func>(f));
    }


/* ==========================================================================
   Pipe Operators
   ========================================================================== */
    /// Default pipe operator
    template <class Data, class Func>
        constexpr auto
        operator|(Data&& x, const Func& f)
        -> decltype(f(std::forward<Data>(x)))
    {
        return f(std::forward<Data>(x));
    }

#ifndef JASNAH_NO_OPTION_SPEC
    /// Specialised pipe operators for working with Option
    template <class Data, class Func, typename U>
    constexpr Jasnah::Option<U>
    operator|(Jasnah::Option<Data>&&x, const Func& f)
    {
        return (!x)
            ? Jasnah::None
            : f(std::forward<Data>(*x));
    }

    template <class Data, class Func>
    constexpr auto
    operator|(Jasnah::Option<Data>&& x, const Func& f)
        -> decltype(f(std::forward<Data>(*x)))
    {
#ifndef JASNAH_NO_EXCEPTIONS
        return (!x)
            ? throw std::invalid_argument(
                "Tried to propagate Jasnah::None through a function not returning Jasnah::Option<T>")
            : f(std::forward<Data>(*x));
#else
        return (!x)
            // The final argument on this branch will not be executed
            // (as abort will not return, but is needed for the return
            // type)
            ? (JAS_LOG"Tried to propagate Jasnah::None through a function not returning Jasnah::Option<T>\n" JAS_LOG_END
               , std::abort()
               , f(std::forward<Data>(*x)))
            : f(std::forward<Data>(*x));
#endif

    }
#endif

/* ==========================================================================
   Curry Operators
   ========================================================================== */
    /// Right Curry operator
    template<typename Func, typename FnArg>
        constexpr auto
        operator<<(const Func& f, FnArg&& fnArg)
        -> decltype(f.template RightCurry<FnArg>(std::forward<FnArg>(fnArg)))
    {
        return f.template RightCurry<FnArg>(std::forward<FnArg>(fnArg));
    }

    /// Left Curry operator
    template<typename Func, typename FnArg>
        constexpr auto
        operator>>(FnArg&& fnArg, const Func& f)
        -> decltype(f.template LeftCurry<FnArg>(std::forward<FnArg>(fnArg)))
    {
        return f.template LeftCurry<FnArg>(std::forward<FnArg>(fnArg));
    }

/* ==========================================================================
   Extension methods for container processing
   ========================================================================== */
#ifndef JASNAH_NO_CTR_LIB
    template <typename T, typename... TArgs, template<typename...>class C, typename F>
    C<T,TArgs...> FilterContainer(const C<T,TArgs...>& ctr, const F& f)
    {
        C<T,TArgs...> result;
        for (const auto& x : ctr)
        {
            if (f(x))
            {
                result.push_back(x);
            }
        }
        return result;
    }
}
#include <vector>
namespace Jasnah
{
    template <typename T, typename... TArgs, typename F>
    std::vector<T,TArgs...> FilterContainer(const std::vector<T,TArgs...>& ctr, const F& f)
    {
        std::vector<T,TArgs...> result;
        result.reserve(ctr.size());
        for (const auto& x : ctr)
        {
            if (f(x))
            {
                result.push_back(x);
            }
        }
        return result;
    }

    template <typename T, typename... TArgs, template <typename...>class C, typename F>
    auto
    MapToContainer(const C<T, TArgs...>& ctr, const F& f)
        -> C<decltype(f(std::declval<T>()))>
    {
        using ResType = decltype(f(std::declval<T>()));
        C<ResType> result;
        for (const auto& x : ctr)
        {
            result.push_back(f(x));
        }
        return result;
    }

    template <typename T, typename... TArgs, typename F>
    auto
    MapToContainer(const std::vector<T, TArgs...>& ctr, const F& f)
        -> std::vector<decltype(f(std::declval<T>()))>
    {
        using ResType = decltype(f(std::declval<T>()));
        std::vector<ResType> result;
        result.reserve(ctr.size());
        for (const auto& x : ctr)
        {
            result.push_back(f(x));
        }
        return result;
    }

    // foldl
    template <typename ResultType, typename InT, typename... InTArgs, template <typename...> class C, typename F>
    ResultType
    ReduceContainer(const C<InT, InTArgs...>& ctr, const ResultType& initial, const F& f)
    {
        ResultType result = initial;
        for (const auto& x : ctr)
        {
            result = f(result, x);
        }
        return result;
    }

JAS_TEMPLATE_FN(Filter, FilterContainer);
JAS_TEMPLATE_FN(Map, MapToContainer);
JAS_TEMPLATE_FN(Reduce, ReduceContainer);

#endif

/* ==========================================================================
   Struct unpacking auto macro stuff
   ========================================================================== */
#ifndef JASNAH_NO_UNPACK
#define JAS_UNPACK1(_1)
#define JAS_UNPACK2(OBJ, NAME1) auto& NAME1(OBJ.NAME1) // Unpacks 1 param
#define JAS_UNPACK3(OBJ, NAME1, NAME2) auto& NAME1(OBJ.NAME1); auto& NAME2(OBJ.NAME2) // Unpacks 2 params
#define JAS_UNPACK4(OBJ, NAME1, NAME2, NAME3) auto& NAME1(OBJ.NAME1); auto& NAME2(OBJ.NAME2); \
    auto& NAME3 = OBJ.NAME3
#define JAS_UNPACK5(OBJ, NAME1, NAME2, NAME3, NAME4) auto& NAME1(OBJ.NAME1); auto& NAME2(OBJ.NAME2); \
    auto& NAME3(OBJ.NAME3); auto& NAME4(OBJ.NAME4)
#define JAS_UNPACK6(OBJ, NAME1, NAME2, NAME3, NAME4, NAME5) auto& NAME1(OBJ.NAME1); auto& NAME2(OBJ.NAME2); \
    auto& NAME3(OBJ.NAME3); auto& NAME4(OBJ.NAME4); auto& NAME5(OBJ.NAME5)

#define JAS_VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, N, ...) N
#define JAS_VA_NARGS(...) JAS_VA_NARGS_IMPL(__VA_ARGS__, 6, 5, 4, 3, 2, 1)


#define JAS_UNPACK_IMPL2(Length, ...) JAS_UNPACK##Length(__VA_ARGS__)
#define JAS_UNPACK_IMPL(Length, ...) JAS_UNPACK_IMPL2(Length, __VA_ARGS__)
#define JasUnpack(...) JAS_UNPACK_IMPL(JAS_VA_NARGS(__VA_ARGS__), __VA_ARGS__)
#endif
}
#endif
