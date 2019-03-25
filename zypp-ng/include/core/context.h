#ifndef ZYPP_NG_CORE_CONTEXT_H_INCLUDED
#define ZYPP_NG_CORE_CONTEXT_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/base.h>
#include <core/signals.h>
#include <memory>


namespace zyppng {

  class Task;
  class ContextPrivate;
  class LIBZYPP_NG_EXPORT Context : public Base
  {
    ZYPP_DECLARE_PRIVATE(Context)
  public:
    using Ptr = std::shared_ptr<Context>;
    using WeakPtr = std::weak_ptr<Context>;
    using TrackPtr = weak_trackable_ptr<Context>;

    Context();

    bool runTask( std::shared_ptr<Task> task );

    SignalProxy<void( )> sigTaskStarted () const;
    SignalProxy<void()> sigTaskFinished () const;
  };
}


#endif // ZYPP_NG_CORE_CONTEXT_H_INCLUDED
