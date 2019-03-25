#ifndef ZYPP_NG_REPO_PRIVATE_REFRESHREPO_P_H_INCLUDED
#define ZYPP_NG_REPO_PRIVATE_REFRESHREPO_P_H_INCLUDED

#include <core/private/task_p.h>
#include <repo/refreshrepo.h>
#include <string>
#include <sigc++/connection.h>

namespace zyppng {

  class RefreshRepoPrivate : public TaskPrivate
  {
  public:
    RefreshRepoPrivate ( std::string &&repo );
    std::string _repoToRefresh;
    sigc::connection _timerConnection;
    int _currProgress = 0;
  };

  class RefreshRepositoriesPrivate : public TaskPrivate
  {
    ZYPP_DECLARE_PUBLIC(RefreshRepositories)
  public:
    std::vector<std::string> _requestedRepos;

    std::unordered_set<RefreshRepo::Ptr> runningRefreshTasks;

    void onRefreshTaskFinished ( std::weak_ptr<Task> rTask );
    void onSubtaskProgress( const Task &, std::string, int );
  };

}


#endif
