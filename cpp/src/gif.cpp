#include "studio/gif.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace studio {
namespace {

// Colours are first grouped into 32x32x32 bins (5 bits per channel).
inline int binOf(const uint8_t* p) { return ((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3); }

struct Box {
  std::vector<int> bins;  // bin ids in this box
  long long weight = 0;
};

void put16(std::ofstream& o, int v) {
  o.put(static_cast<char>(v & 0xff));
  o.put(static_cast<char>((v >> 8) & 0xff));
}

/** Packs variable-width LZW codes into GIF's 255-byte sub-blocks. */
class BitWriter {
 public:
  explicit BitWriter(std::ofstream& o) : out_(o) {}
  void write(int code, int width) {
    acc_ |= static_cast<uint32_t>(code) << bits_;
    bits_ += width;
    while (bits_ >= 8) {
      byte(static_cast<uint8_t>(acc_ & 0xff));
      acc_ >>= 8;
      bits_ -= 8;
    }
  }
  void finish() {
    if (bits_ > 0) byte(static_cast<uint8_t>(acc_ & 0xff));
    acc_ = 0;
    bits_ = 0;
    flushBlock();
    out_.put(0);  // block terminator
  }

 private:
  void byte(uint8_t b) {
    block_[len_++] = b;
    if (len_ == 255) flushBlock();
  }
  void flushBlock() {
    if (len_ == 0) return;
    out_.put(static_cast<char>(len_));
    out_.write(reinterpret_cast<const char*>(block_.data()), len_);
    len_ = 0;
  }
  std::ofstream& out_;
  uint32_t acc_ = 0;
  int bits_ = 0;
  std::array<uint8_t, 255> block_{};
  int len_ = 0;
};

void lzwEncode(std::ofstream& out, const std::vector<uint8_t>& indices) {
  constexpr int kMinCode = 8, kClear = 256, kEnd = 257, kMaxCode = 4096;
  out.put(kMinCode);
  BitWriter bw(out);
  // dict[prefix * 256 + byte] = code (0 means absent).
  std::vector<uint16_t> dict(static_cast<size_t>(kMaxCode) * 256, 0);
  int width = kMinCode + 1;
  int next = kEnd + 1;
  bw.write(kClear, width);
  if (indices.empty()) {
    bw.write(kEnd, width);
    bw.finish();
    return;
  }
  int prefix = indices[0];
  for (size_t i = 1; i < indices.size(); ++i) {
    const int c = indices[i];
    const size_t key = static_cast<size_t>(prefix) * 256 + static_cast<size_t>(c);
    if (dict[key]) {
      prefix = dict[key];
      continue;
    }
    bw.write(prefix, width);
    dict[key] = static_cast<uint16_t>(next);
    // The new code needs one more bit once it no longer fits (the decoder widens in step).
    if (next >= (1 << width) && width < 12) ++width;
    ++next;
    if (next == kMaxCode) {
      // Table full: start over, as most encoders do, so every decoder stays in step.
      bw.write(kClear, width);
      std::fill(dict.begin(), dict.end(), 0);
      width = kMinCode + 1;
      next = kEnd + 1;
    }
    prefix = c;
  }
  bw.write(prefix, width);
  bw.write(kEnd, width);
  bw.finish();
}

}  // namespace

std::vector<uint8_t> quantize(const uint8_t* rgba, int pixelCount, int maxColors, std::vector<uint8_t>& palette) {
  std::vector<int> hist(32768, 0);
  for (int i = 0; i < pixelCount; ++i) hist[binOf(rgba + i * 4)]++;

  Box all;
  for (int b = 0; b < 32768; ++b)
    if (hist[b]) {
      all.bins.push_back(b);
      all.weight += hist[b];
    }
  std::vector<Box> boxes{std::move(all)};

  const auto channel = [](int bin, int ch) { return ch == 0 ? (bin >> 10) & 31 : ch == 1 ? (bin >> 5) & 31 : bin & 31; };

  // Median cut: keep splitting the heaviest box that can be split along its widest channel.
  while (static_cast<int>(boxes.size()) < maxColors) {
    int pick = -1;
    long long best = -1;
    int pickCh = 0;
    for (size_t i = 0; i < boxes.size(); ++i) {
      if (boxes[i].bins.size() < 2) continue;
      int lo[3] = {31, 31, 31}, hi[3] = {0, 0, 0};
      for (int b : boxes[i].bins)
        for (int ch = 0; ch < 3; ++ch) {
          lo[ch] = std::min(lo[ch], channel(b, ch));
          hi[ch] = std::max(hi[ch], channel(b, ch));
        }
      int ch = 0;
      for (int k = 1; k < 3; ++k)
        if (hi[k] - lo[k] > hi[ch] - lo[ch]) ch = k;
      const long long score = boxes[i].weight * (hi[ch] - lo[ch] + 1);
      if (score > best) {
        best = score;
        pick = static_cast<int>(i);
        pickCh = ch;
      }
    }
    if (pick < 0) break;
    Box& box = boxes[pick];
    std::sort(box.bins.begin(), box.bins.end(),
              [&](int a, int b) { return channel(a, pickCh) < channel(b, pickCh); });
    long long half = box.weight / 2, run = 0;
    size_t cut = 1;
    for (size_t k = 0; k + 1 < box.bins.size(); ++k) {
      run += hist[box.bins[k]];
      cut = k + 1;
      if (run >= half) break;
    }
    Box second;
    second.bins.assign(box.bins.begin() + static_cast<long>(cut), box.bins.end());
    box.bins.resize(cut);
    box.weight = 0;
    for (int b : box.bins) box.weight += hist[b];
    for (int b : second.bins) second.weight += hist[b];
    boxes.push_back(std::move(second));
  }

  // Each box's colour is the weighted mean of its pixels (using the exact pixel values).
  std::vector<int> binToBox(32768, 0);
  for (size_t i = 0; i < boxes.size(); ++i)
    for (int b : boxes[i].bins) binToBox[b] = static_cast<int>(i);
  std::vector<long long> sum(boxes.size() * 3, 0), count(boxes.size(), 0);
  for (int i = 0; i < pixelCount; ++i) {
    const uint8_t* p = rgba + i * 4;
    const int box = binToBox[binOf(p)];
    sum[box * 3] += p[0];
    sum[box * 3 + 1] += p[1];
    sum[box * 3 + 2] += p[2];
    count[box]++;
  }
  palette.assign(256 * 3, 0);
  for (size_t i = 0; i < boxes.size(); ++i)
    if (count[i])
      for (int ch = 0; ch < 3; ++ch) palette[i * 3 + ch] = static_cast<uint8_t>(sum[i * 3 + ch] / count[i]);

  std::vector<uint8_t> indices(static_cast<size_t>(pixelCount));
  for (int i = 0; i < pixelCount; ++i) indices[i] = static_cast<uint8_t>(binToBox[binOf(rgba + i * 4)]);
  return indices;
}

GifWriter::GifWriter(const std::string& path, int width, int height)
    : out_(path, std::ios::binary), width_(width), height_(height) {
  if (!out_) throw std::runtime_error("Could not write " + path);
  if (width <= 0 || height <= 0 || width > 65535 || height > 65535) throw std::invalid_argument("bad GIF size");
  out_.write("GIF89a", 6);
  put16(out_, width);
  put16(out_, height);
  out_.put(0);  // no global colour table
  out_.put(0);  // background colour
  out_.put(0);  // pixel aspect ratio
  // NETSCAPE2.0 extension: loop forever.
  const unsigned char loop[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
                                0x03, 0x01, 0x00, 0x00, 0x00};
  out_.write(reinterpret_cast<const char*>(loop), sizeof loop);
}

GifWriter::~GifWriter() {
  try {
    close();
  } catch (...) {
  }
}

void GifWriter::addFrame(const uint8_t* rgba, int delayCs) {
  if (closed_) throw std::logic_error("GifWriter is closed");
  std::vector<uint8_t> palette;
  const std::vector<uint8_t> indices = quantize(rgba, width_ * height_, 256, palette);

  // Graphic control extension: frame delay, each frame replaces the last.
  out_.put(0x21);
  out_.put(static_cast<char>(0xF9));
  out_.put(4);
  out_.put(0x04);
  put16(out_, std::max(2, delayCs));
  out_.put(0);
  out_.put(0);
  // Image descriptor with a 256-entry local colour table.
  out_.put(0x2C);
  put16(out_, 0);
  put16(out_, 0);
  put16(out_, width_);
  put16(out_, height_);
  out_.put(static_cast<char>(0x80 | 7));
  out_.write(reinterpret_cast<const char*>(palette.data()), 768);
  lzwEncode(out_, indices);
  if (!out_) throw std::runtime_error("Could not write the GIF");
}

void GifWriter::close() {
  if (closed_) return;
  closed_ = true;
  out_.put(0x3B);
  out_.close();
}

}  // namespace studio
