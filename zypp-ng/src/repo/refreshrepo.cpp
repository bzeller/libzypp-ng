#include "repo/refreshrepo.h"
#include "repo/private/refreshrepo_p.h"
#include "core/context.h"
#include <glibmm/timer.h>
#include <glibmm/main.h>

namespace zyppng {

  RefreshRepoPrivate::RefreshRepoPrivate( std::string &&repo )
    : _repoToRefresh( std::move(repo) )
  {

  }

  RefreshRepo::RefreshRepo( std::string repo )
    : Task ( * new RefreshRepoPrivate( std::move( repo ) ) )
  {

  }

  IdString RefreshRepo::taskType() const
  {
    return IdString("Zypp.Task.RefreshRepo");
  }

  void RefreshRepo::cancel()
  {
    Z_D();
    d->_timerConnection.disconnect();
    d->_error.emit( *this, Error{"Task cancelled by user"} );
    finalize();
  }

  void RefreshRepo::run( std::weak_ptr<Context> )
  {
    //progress the task every second
    d_func()->_timerConnection = Glib::signal_timeout().connect_seconds( sigc::mem_fun( *this, &RefreshRepo::advanceSimulation ), 1 );
  }

  bool RefreshRepo::advanceSimulation()
  {
    Z_D();

    d->_currProgress+=20;

    std::stringstream s;
    s << "Refreshing " << d->_repoToRefresh;

    updateProgress( s.str(), d->_currProgress );

    if ( d->_currProgress >= 100 ) {
      finalize();
      return false;
    }
    return true;
  }


  RefreshRepositories::RefreshRepositories( const std::vector<std::string> &repos )
    : Task ( * new RefreshRepositoriesPrivate )
  {
    d_func()->_requestedRepos = repos;
  }

  RefreshRepositories::~RefreshRepositories()
  {
  }

  void RefreshRepositoriesPrivate::onSubtaskProgress( const Task &t, std::string, int p)
  {
    Z_Z();

    int done = _requestedRepos.size() - runningRefreshTasks.size();

    int accProgress = done * 100;
    for ( RefreshRepo::Ptr running : runningRefreshTasks ) {
      accProgress += running->progress();
    }

    int percentageHack =  accProgress / _requestedRepos.size();
    z->updateProgress( "Refresh running", percentageHack );
  }

  void RefreshRepositoriesPrivate::onRefreshTaskFinished( std::weak_ptr<Task> rTask )
  {
    Z_Z();

    std::shared_ptr<Task> t = rTask.lock();
    if ( !t )
      return;

    //we do not know that task
    if ( runningRefreshTasks.find(t) == runningRefreshTasks.end() )
      return;

    runningRefreshTasks.erase( t );

    //once the last one has ended , finalize the object, we are done
    if ( runningRefreshTasks.empty() ) {
      z->updateProgress( "Refreshing finished", 100 );
      z->finalize();
    }
  }

  zyppng::IdString RefreshRepositories::taskType() const
  {
    return IdString("Zypp.Task.RefreshRepositories");
  }

  void RefreshRepositories::cancel()
  {
    //TODO cancel all tasks
  }

  void RefreshRepositories::run( std::weak_ptr<zyppng::Context> )
  {
    Z_D();

    //nothing to do
    if ( d->_requestedRepos.empty() ) {
      finalize();
      return;
    }

    for ( const std::string &repo : d->_requestedRepos ) {
      RefreshRepo::Ptr ref( new RefreshRepo( repo ) );

      d->runningRefreshTasks.insert( ref );

      ref->sigProgressChanged().connect( sigc::mem_fun( *d, &RefreshRepositoriesPrivate::onSubtaskProgress ) );
      ref->sigFinished().connect( sigc::mem_fun( *d, &RefreshRepositoriesPrivate::onRefreshTaskFinished ) );

      d->_subTaskStarted.emit( shared_this<RefreshRepositories>(), ref );
      ref->start( d->_myContext );
    }

    updateProgress( "Refresh started", 0 );
  }

} // namespace zyppng
