#include "storage/test_backend.h"
#include "libzlog/test_libzlog.h"
#include "include/zlog/backend/lmdb.h"
#include "port/stack_trace.h"

void BackendTest::SetUp() {}
void BackendTest::TearDown() {}

struct DBPathContext {
  char *dbpath = nullptr;
  virtual ~DBPathContext() {
    if (dbpath) {
      struct stat st;
      if (stat(dbpath, &st) == 0) {
        char cmd[PATH_MAX];
        sprintf(cmd, "rm -rf %s", dbpath);
        EXPECT_EQ(system(cmd), 0);
      }
      free(dbpath);
    }
  }
};

struct LibZLogTest::Context : public DBPathContext {
  LMDBBackend *backend = nullptr;
  ~Context() {
    if (backend) {
      backend->Close();
      delete backend;
    }
  }
};

void LibZLogTest::SetUp() {
  context = new Context;

  context->dbpath = strdup("/tmp/zlog.db.XXXXXX");
  ASSERT_NE(mkdtemp(context->dbpath), nullptr);
  ASSERT_GT(strlen(context->dbpath), (unsigned)0);

  if (lowlevel()) {
    context->backend = new LMDBBackend();
    context->backend->Init(context->dbpath, true);
    int ret = zlog::Log::Create(context->backend,
        "mylog", NULL, &log);
    ASSERT_EQ(ret, 0);
  } else {
    int ret = zlog::Log::Create("lmdb", "mylog",
        {{"path", context->dbpath}}, &log);
    ASSERT_EQ(ret, 0);
  }
}

void LibZLogTest::TearDown() {
  if (log)
    delete log;
  if (context)
    delete context;
}

struct LibZLogCAPITest::Context : public DBPathContext {
  zlog_backend_t backend = nullptr;
  ~Context() {
    if (backend) {
      zlog_destroy_lmdb_backend(backend);
    }
  }
};

void LibZLogCAPITest::SetUp() {
  context = new Context;

  context->dbpath = strdup("/tmp/zlog.db.XXXXXX");
  ASSERT_NE(mkdtemp(context->dbpath), nullptr);
  ASSERT_GT(strlen(context->dbpath), (unsigned)0);

  if (lowlevel()) {
    int ret = zlog_create_lmdb_backend(context->dbpath,
        &context->backend);
    ASSERT_EQ(ret, 0);
    ret = zlog_create(context->backend, "c_mylog",
        NULL, &log);
    ASSERT_EQ(ret, 0);
  } else {
    const char *keys[] = {"path"};
    const char *vals[] = {context->dbpath};
    int ret = zlog_create_nobe("lmdb", "c_mylog",
        keys, vals, 1, &log);
    ASSERT_EQ(ret, 0);
  }
}

void LibZLogCAPITest::TearDown() {
  if (log)
    zlog_destroy(log);

  if (context)
    delete context;
}

INSTANTIATE_TEST_CASE_P(Level, LibZLogTest,
    ::testing::Values(true, false));

INSTANTIATE_TEST_CASE_P(LevelCAPI, LibZLogCAPITest,
    ::testing::Values(true, false));

int main(int argc, char **argv)
{
  rocksdb::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
