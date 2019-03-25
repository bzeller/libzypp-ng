#ifndef ZYPP_NG_CORE_PRIVATE_CONTEXT_P_H_INCLUDED
#define ZYPP_NG_CORE_PRIVATE_CONTEXT_P_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/private/base_p.h>
#include <core/context.h>
#include <core/task.h>

#include <boost/shared_ptr.hpp>
#include <queue>

namespace zypp {
  class ZYpp;
}

namespace zyppng {

  class Task;

  class ContextPrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(Context)
  public:

    ContextPrivate();

    Task::Ptr currentTask;
    connection taskFinishedConnection;

    boost::shared_ptr<zypp::ZYpp> zyppApi;

  private:
    void onTaskFinished ( Task::WeakPtr task );
    bool onIdle();
  };
}


#endif
