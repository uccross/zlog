#include "db_impl.h"

TransactionImpl::TransactionImpl(DBImpl *db,
    const std::shared_ptr<NodePtr>& snapshot, uint64_t rid) :
  db_(db)
{
  intention_ = std::make_shared<Intention>(snapshot, rid);
}

void TransactionImpl::Put(const std::string& key, const std::string& val)
{
  intention_->Put(key, val);
}

void TransactionImpl::Delete(std::string key)
{
  intention_->Delete(key);
}

bool TransactionImpl::Commit()
{
  // nothing to do
  if (intention_->Empty())
    return true;

  // build the intention and fixup field offsets
  //assert(root_ != nullptr);
  //if (root_ == Node::Nil()) {
  //} else
  //  assert(root_->rid() == rid_);

  // append to the database log
  std::string blob;
  intention_->Serialize(&blob);

  uint64_t pos;
  int ret = db_->log_->Append(blob, &pos);
  assert(ret == 0);
  db_->log_cond_.notify_all();

  // update the in-memory intention ptrs
  intention_->SetCSN(pos);

  // wait for result
  bool committed = db_->CommitResult(pos);
  return committed;
}
