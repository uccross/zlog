#ifndef ZLOG_KVSTORE_TRANSACTION_H
#define ZLOG_KVSTORE_TRANSACTION_H
#include <deque>
#include "node.h"
#include "kvstore/kvstore.pb.h"
#include "zlog/transaction.h"
#include "intention.h"

class DBImpl;

/*
 * A transaction is a light-weight handle that references an intention.
 */
class TransactionImpl : public Transaction {
 public:
  TransactionImpl(DBImpl *db, const std::shared_ptr<NodePtr>& snapshot, uint64_t rid);

  void Put(const std::string& key, const std::string& val);
  void Delete(std::string key);
  bool Commit();

 private:
  DBImpl *db_;
  std::shared_ptr<Intention> intention_;
};

#endif
