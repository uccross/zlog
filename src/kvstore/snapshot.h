#ifndef ZLOG_KVSTORE_SNAPSHOT_H
#define ZLOG_KVSTORE_SNAPSHOT_H
#include <cstdint>
#include <string>
#include <vector>
#include "node.h"

class Snapshot {
 public:
  Snapshot(const std::shared_ptr<NodePtr>& root, std::vector<std::string> desc) :
    root(root), desc(desc)
  {}

  const std::shared_ptr<NodePtr> root;

  // TODO: remove in favor of some sort of pointer to this state. for example
  // let's have a special RootNodeRef that has additional metadata or
  // something along those lines.
  const std::vector<std::string> desc;
};

#endif
