#pragma once

#include <vector>

#include "studio/runner.hpp"

namespace studio {

/** Returns canned replies without calling any model.
 *  Used by tests and handy for trying the app offline. */
class MockRunner : public AgentRunner {
 public:
  /** @param rejectFirstReviews how many reviews ask for fixes before approving */
  explicit MockRunner(int rejectFirstReviews = 0) : rejectFirstReviews_(rejectFirstReviews) {}
  std::string name() const override { return "mock"; }
  std::string run(const AgentRequest& request) override;

  std::vector<AgentRequest> calls;

 private:
  int rejectFirstReviews_;
  int reviewCount_ = 0;
};

}  // namespace studio
