#include "studio/parse.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace studio {
namespace {

std::string toLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
  while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
  return s;
}

/** Text inside the first ``` fence whose opening matches `tag` (empty tag: any fence).
 *  The tag is matched case-insensitively. Returns false when there is no closed fence. */
bool fenced(std::string_view text, std::string_view tag, std::string_view& inside) {
  const std::string lower = toLower(text);
  const std::string open = "```" + toLower(tag);
  size_t start = lower.find(open);
  if (start == std::string::npos) return false;
  size_t body = start + open.size();
  if (tag.empty()) {
    // Like the regex ```(?:json)?\s* : skip an optional "json" tag.
    if (lower.compare(body, 4, "json") == 0) body += 4;
  }
  while (body < text.size() && isSpace(text[body])) ++body;
  size_t end = text.find("```", body);
  if (end == std::string::npos) return false;
  inside = text.substr(body, end - body);
  return true;
}

}  // namespace

nlohmann::json extractJson(std::string_view text, std::string_view agent) {
  std::string_view candidate = text;
  std::string_view inside;
  if (fenced(text, "", inside)) candidate = inside;

  const size_t start = candidate.find('{');
  const size_t end = candidate.rfind('}');
  if (start == std::string_view::npos || end == std::string_view::npos || end <= start) {
    throw std::runtime_error(std::string(agent) + " agent did not return JSON.");
  }
  try {
    return nlohmann::json::parse(candidate.substr(start, end - start + 1));
  } catch (const nlohmann::json::exception& err) {
    throw std::runtime_error(std::string(agent) + " agent returned invalid JSON: " + err.what());
  }
}

std::string extractHtml(std::string_view text) {
  std::string_view inside;
  std::string_view html = trim(fenced(text, "html", inside) ? inside : text);
  const std::string lower = toLower(html);
  const size_t doctype = lower.find("<!doctype html");
  const size_t tag = lower.find("<html");
  const size_t start = std::min(doctype, tag);
  if (start != std::string::npos && start > 0) html.remove_prefix(start);
  return std::string(html);
}

}  // namespace studio
