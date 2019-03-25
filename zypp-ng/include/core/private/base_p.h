#ifndef ZYPP_NG_CORE_PRIVATE_BASE_P_H_INCLUDED
#define ZYPP_NG_CORE_PRIVATE_BASE_P_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/base.h>
#include <core/signals.h>
#include <unordered_set>

namespace zyppng
{

  class BasePrivate : public sigc::trackable
  {
    ZYPP_DECLARE_PUBLIC(Base)
  public:
    virtual ~BasePrivate();
    Base::TrackPtr parent { nullptr };
    std::unordered_set< Base::Ptr > children;
    Base *z_ptr = nullptr;
  };

}


#endif
