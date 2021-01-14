/*
 *  Copyright (c) 2020 Intel Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#ifndef _ONEDPL_FUTURE_HPP
#define _ONEDPL_FUTURE_HPP

namespace oneapi
{
namespace dpl
{
namespace __internal
{

// TODO: Rework async data structures for return value
template <typename _Tp>
struct __async_value
{
    virtual _Tp
    data() = 0;
    virtual ~__async_value() = default;
    virtual sycl::buffer<int>
    raw_data() const;
};

template <typename _Tp>
struct __async_direct : public __async_value<_Tp>
{
    using _T = typename std::iterator_traits<_Tp>::value_type;
    _Tp __data;
    __async_direct(_Tp _d) : __data(_d) {}
    _Tp data() { return __data; }
    sycl::buffer<_T>
    raw_data() const
    {
        return __data.get_buffer();
    }
};

template <typename _Tp, typename _Op = ::std::plus<_Tp>, typename _Buf = sycl::buffer<_Tp>>
struct __async_indirect : public __async_value<_Tp>
{
    _Buf __buf;
    _Op __op;
    _Tp __init;
    __async_indirect(_Buf _b, _Tp _v = 0, _Op _o = std::plus<_Tp>()) : __buf(_b), __op(_o), __init(_v) {}
    _Tp
    data()
    {
        auto ret_val = __buf.template get_access<access_mode::read>()[0];
        return __op(ret_val, __init);
    }
    sycl::buffer<_Tp>
    raw_data() const
    {
        return __buf;
    }
};

struct _tmp_base
{
};

template <typename... Ts>
struct _TempObjs : public _tmp_base
{
    std::tuple<Ts&...> __my_tmps;
    _TempObjs(Ts&... __t) : __my_tmps(::std::forward_as_tuple(__t...)) {}
};

#if 1

using event = sycl::event;

#else

class event {
    sycl::event __event;
    event( sycl::event __e ) : __event(__e) {}
}

#endif

class __future_base {
    event __my_event;
public:
    __future_base(event __e) : __my_event(__e) {}
    event get_event() const { return __my_event; }
    void wait() { __my_event.wait(); }
    operator event() const { return event(__my_event); }
};

// TODO: pull future into public API

template <typename _Tp>
class __future : public __future_base
{
    ::std::unique_ptr<__async_value<_Tp>> __data; // This is a value/buffer for read access!
    _tmp_base __tmp;

  public:
    template <typename... _Ts>
    __future(event __e, _Tp __d, _Ts... __t)
        : __future_base(__e), __data(::std::unique_ptr<__async_direct<_Tp>>(new __async_direct<_Tp>(__d))),
          __tmp(_TempObjs<_Ts...>{__t...})
    {
    }
    template <typename... _Ts>
    __future(event __e, sycl::buffer<_Tp> __d, _Ts... __t)
        : __future_base(__e), __data(::std::unique_ptr<__async_indirect<_Tp>>(new __async_indirect<_Tp>(__d))),
          __tmp(_TempObjs<_Ts...>{__t...})
    {
    }
    // Constructor for reduce_transform pattern
    template <typename _Op>
    __future(const __future<_Tp>& _fp, _Tp __i, _Op __o)
        : __future_base(_fp.get_event()),
          __data(::std::unique_ptr<__async_indirect<_Tp, _Op>>(new __async_indirect<_Tp, _Op>(_fp.raw_data(), __i, __o))), __tmp(_fp.__tmp)
    {
    }
    // Return underlying buffer
    auto
    raw_data() const
    {
        return __data->raw_data();
    }
    _Tp
    data() const
    {
        return __data->data();
    }
};

template<>
class __future<void> : public __internal::__future_base
{
    _tmp_base __tmp;

  public:
    template <typename... _Ts>
    __future(event __e, _Ts... __t)
        : __future_base(__e), __tmp(_TempObjs<_Ts...>{__t...})
    {
    }
};

} // end namespace __internal

}}

#endif