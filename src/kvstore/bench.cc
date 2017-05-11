#include <sstream>
#include <thread>
#include <random>
#include <iostream>
#include <iomanip>
#include <map>
#include <cstdlib>
#include <boost/program_options.hpp>
#include <mutex>
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include "zlog/db.h"
#include "zlog/backend/lmdb.h"
#include "zlog/backend/fakeseqr.h"

namespace po = boost::program_options;

#if __APPLE__
static inline uint64_t getns()
{
  struct timeval tv;
  assert(gettimeofday(&tv, NULL) == 0);
  uint64_t res = tv.tv_sec * 1000000000ULL;
  return res + tv.tv_usec * 1000ULL;
}
#else
static inline uint64_t __getns(clockid_t clock)
{
  struct timespec ts;
  int ret = clock_gettime(clock, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}
static inline uint64_t getns()
{
  return __getns(CLOCK_MONOTONIC);
}
#endif

struct txn_stat {
  uint64_t ns_begin_latency;
  uint64_t ns_run_latency;
  uint64_t ns_commit_latency;
};

static std::mutex lock;
static std::vector<txn_stat> stats;

static inline std::string tostr(int value)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << value;
  return ss.str();
}

static void report(std::string logfile)
{
  std::ofstream out;
  bool write_log = !logfile.empty();
  if (write_log) {
    out.open(logfile, std::ios::trunc);
    out << "ns_begin_lat,ns_run_lat,ns_commit_lat" << std::endl;
  }
  while (true) {
    std::vector<txn_stat> last_stats;
    last_stats.swap(stats);
    if (!last_stats.empty()) {
      if (write_log) {
        for (const auto& s : last_stats) {
          out << s.ns_begin_latency << ","
            << s.ns_run_latency << ","
            << s.ns_commit_latency << std::endl;
        }
      }
    }
    sleep(1);
  }
}

int main(int argc, char **argv)
{
  int txn_size;
  std::string logfile;

  po::options_description gen_opts("General options");
  gen_opts.add_options()
    ("help,h", "show help message")
    ("log-file", po::value<std::string>(&logfile)->default_value(""), "log file")
    ("txn-size", po::value<int>(&txn_size)->default_value(1), "txn size")
  ;

  po::options_description all_opts("Allowed options");
  all_opts.add(gen_opts);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, all_opts), vm);

  if (vm.count("help")) {
    std::cout << all_opts << std::endl;
    return 1;
  }

  po::notify(vm);

  assert(txn_size > 0);

  zlog::Log *log;
  auto be = new LMDBBackend();
  be->Init();
  auto client = new FakeSeqrClient();
  int ret = zlog::Log::Create(be, "log", client, &log);
  assert(ret == 0);

  DB *db;
  ret = DB::Open(log, true, &db);
  assert(ret == 0);

  std::thread report_thread(report, logfile);

  std::mt19937 gen(0);
  std::uniform_int_distribution<unsigned long long> dis;

  while (true) {
    // create txn context
    auto ns_begin_txn = getns();
    auto txn = db->BeginTransaction();

    // run txn: get/put/del/etc...
    auto ns_begin_txn_run = getns();
    for (int i = 0; i < txn_size; i++) {
      const std::string key = tostr(dis(gen));
      txn->Put(key, key);
    }

    // commit txn
    auto ns_begin_txn_commit = getns();
    txn->Commit();
    auto ns_end_txn = getns();
    delete txn;

    struct txn_stat s;
    s.ns_begin_latency = ns_begin_txn_run - ns_begin_txn;
    s.ns_run_latency = ns_begin_txn_commit - ns_begin_txn_run;
    s.ns_commit_latency = ns_end_txn - ns_begin_txn_commit;

    std::lock_guard<std::mutex> l(lock);
    stats.push_back(s);
  }

  report_thread.join();

  delete db;
  delete log;
  delete client;
  be->Close();
  delete be;
}
