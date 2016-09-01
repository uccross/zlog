#include "node_cache.h"
#include "db_impl.h"

void NodeCache::ResolveNodePtr(NodePtr& ptr)
{
  if (ptr.ref() != nullptr)
    return;

  auto it = nodes_.find(std::make_pair(ptr.csn(), ptr.offset()));
  if (it != nodes_.end()) {
    ptr.set_ref(it->second);
    return;
  }

  /*
   * Resolving nodes into the log is currently disabled because everything is
   * cached in memory. One challenge of resolving nodes populating their
   * version number field which isn't store in the log. This is achieved by
   * storing extra metadata in nodes and pointers that allow a traversal to
   * find out what the version number is but blindly indexing into the log
   * won't work as expected.
   */
  assert(0);

  // the cache sits on top of the database log
  std::string snapshot;
  int ret = db_->log_->Read(ptr.csn(), &snapshot);
  assert(ret == 0);

  kvstore_proto::Intention i;
  assert(i.ParseFromString(snapshot));
  assert(i.IsInitialized());

  auto nn = deserialize_node(i, ptr.csn(), ptr.offset(), 0);

  assert(nn->read_only());
  nodes_.insert(std::make_pair(
        std::make_pair(ptr.csn(), ptr.offset()), nn));

  ptr.set_ref(nn);
}

NodeRef NodeCache::CacheIntention(const kvstore_proto::Intention& i,
    uint64_t pos, uint64_t& lcs_csn)
{
  if (i.tree_size() == 0)
    return Node::Nil();

  NodeRef nn = nullptr;
  for (int idx = 0; idx < i.tree_size(); idx++) {
    //std::cerr << "setting vn: prev " << lcs_csn << " vn " << lcs_csn + idx + 1 << std::endl;
    nn = deserialize_node(i, pos, idx, lcs_csn + idx + 1);

    assert(nn->read_only());
    nodes_.insert(std::make_pair(std::make_pair(pos, idx), nn));
  }

  lcs_csn = lcs_csn + i.tree_size();

  assert(nn != nullptr);
  return nn; // root is last node in intention
}

NodeRef NodeCache::deserialize_node(const kvstore_proto::Intention& i,
    uint64_t pos, int index, uint64_t vn)
{
  const kvstore_proto::Node& n = i.tree(index);

  // TODO: replace rid==csn with a lookup table that lets us
  // use random values for more reliable assertions.
  //
  // TODO: initialize so it can be read-only after creation
  auto nn = std::make_shared<Node>(n.key(), n.val(), n.red(),
      Node::Nil(), Node::Nil(), pos, index, false, n.altered(),
      n.depends());

  if (n.has_ssv())
    nn->set_ssv(n.ssv());
  nn->set_subtree_ro_dependent(n.subtree_ro_dependent());
  nn->set_vn(vn);

  assert(nn->field_index() == index);
  if (!n.left().nil()) {
    nn->left.set_ref(nullptr);
    nn->left.set_offset(n.left().off());
    if (n.left().self()) {
      nn->left.set_csn(pos);
    } else {
      nn->left.set_csn(n.left().csn());
    }
    ResolveNodePtr(nn->left);
  }

  if (!n.right().nil()) {
    nn->right.set_ref(nullptr);
    nn->right.set_offset(n.right().off());
    if (n.right().self()) {
      nn->right.set_csn(pos);
    } else {
      nn->right.set_csn(n.right().csn());
    }
    ResolveNodePtr(nn->right);
  }

  nn->set_read_only();

  return nn;
}
