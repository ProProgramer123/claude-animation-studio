#include "studio/paths.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string>

namespace studio {

std::string slugify(std::string_view prompt) {
  std::string slug;
  bool dash = false;
  for (char raw : prompt) {
    const auto c = static_cast<unsigned char>(raw);
    if (std::isalnum(c) && c < 128) {
      if (dash && !slug.empty()) slug += '-';
      dash = false;
      slug += static_cast<char>(std::tolower(c));
    } else {
      dash = true;
    }
  }
  if (slug.size() > 40) slug.resize(40);
  while (!slug.empty() && slug.back() == '-') slug.pop_back();
  return slug.empty() ? "animation" : slug;
}

std::filesystem::path animationDir(std::string_view prompt, std::filesystem::path base) {
  if (base.empty()) {
    const char* env = std::getenv("ANIMATION_STUDIO_OUTPUT_DIR");
    base = (env && *env) ? std::filesystem::path(env) : std::filesystem::path("animations");
  }
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  char stamp[32];
  std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H-%M-%S", &utc);
  return std::filesystem::absolute(base / (std::string(stamp) + "-" + slugify(prompt)));
}

}  // namespace studio
