#include "core/context.h"
#include "core/private/context_p.h"
#include "core/private/base_p.h"

#include <zypp/ZYppFactory.h>

#include <glibmm/main.h>


namespace zyppng {

  ContextPrivate::ContextPrivate()
  {
    zyppApi = zypp::getZYpp();
  }

  void ContextPrivate::onTaskFinished( Task::WeakPtr task )
  {
    Task::Ptr lockedTask = task.lock();
    if ( lockedTask == currentTask ) {

      taskFinishedConnection.disconnect();

      // clear reference counter on Task, this might delete the object
      currentTask.reset();
    }
  }

  bool ContextPrivate::onIdle()
  {
    taskFinishedConnection = currentTask->sigFinished().connect( sigc::mem_fun( *this, &ContextPrivate::onTaskFinished ) );
    currentTask->start( z_ptr->shared_this<Context>() );

    //lets not get called again
    return false;
  }

  Context::Context() : Base ( *new ContextPrivate )
  {

  }

  bool Context::runTask( std::shared_ptr<Task> task )
  {
    Z_D();
    if ( d->currentTask )
      return d->currentTask == task;

    d->currentTask = task;

    Glib::RefPtr<Glib::MainContext> ctx = Glib::MainContext::get_default();
    if ( !ctx )
      return false;

    //make sure the task is not started before the main loop was executed
    ctx->signal_idle().connect( sigc::mem_fun( *d_func(), &ContextPrivate::onIdle ) );
    return true;
  }

}
