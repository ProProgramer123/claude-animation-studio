#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace studio {

/** Writes an animated GIF one frame at a time, so long animations never sit in memory.
 *  Each frame gets its own 256-colour palette (median cut), which keeps gradients smooth.
 *  The GIF loops forever. Throws std::runtime_error if the file can't be written. */
class GifWriter {
 public:
  GifWriter(const std::string& path, int width, int height);
  ~GifWriter();

  /** `rgba` holds width*height pixels, 4 bytes each (alpha is ignored).
   *  `delayCs` is how long the frame shows, in hundredths of a second (2 or more). */
  void addFrame(const uint8_t* rgba, int delayCs);
  /** Writes the trailer. Called by the destructor if you forget. */
  void close();

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  std::ofstream out_;
  int width_, height_;
  bool closed_ = false;
};

/** Reduces an RGBA image to at most `maxColors` colours. Fills `palette` (r,g,b triples) and
 *  returns one palette index per pixel. Exposed for tests. */
std::vector<uint8_t> quantize(const uint8_t* rgba, int pixelCount, int maxColors, std::vector<uint8_t>& palette);

}  // namespace studio
