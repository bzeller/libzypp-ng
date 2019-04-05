#ifndef ZYPP_NG_CORE_ZYPPGLOBAL_H_INCLUDED
#define ZYPP_NG_CORE_ZYPPGLOBAL_H_INCLUDED

#include <zypp-ng_export.h>
#include <zypp/base/Easy.h>

/*
 * \note those macros are inspired by the Qt framework
 */

template <typename T> inline T *zyppGetPtrHelper(T *ptr) { return ptr; }
template <typename Ptr> inline auto zyppGetPtrHelper(const Ptr &ptr) -> decltype(ptr.operator->()) { return ptr.operator->(); }
template <typename Ptr> inline auto zyppGetPtrHelper(Ptr &ptr) -> decltype(ptr.operator->()) { return ptr.operator->(); }

#define ZYPP_DECLARE_PRIVATE(Class) \
    inline Class##Private* d_func() \
    { return reinterpret_cast<Class##Private *>(zyppGetPtrHelper(d_ptr)); } \
    inline const Class##Private* d_func() const \
    { return reinterpret_cast<const Class##Private *>(zyppGetPtrHelper(d_ptr)); } \
    friend class Class##Private;

#define ZYPP_DECLARE_PUBLIC(Class)                                    \
    inline Class* z_func() { return static_cast<Class *>(z_ptr); } \
    inline const Class* z_func() const { return static_cast<const Class *>(z_ptr); } \
    friend class Class;

#define Z_D() auto const d = d_func()
#define Z_Z() auto const z = z_func()

#endif
