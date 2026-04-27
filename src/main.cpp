#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kGlyphCount = 48;
constexpr float kPi = 3.14159265358979323846f;

using Glyph = std::array<std::uint8_t, kGlyphHeight>;
using GlyphPattern = std::array<std::string_view, kGlyphHeight>;

enum class GlyphSet {
  Procedural,
  PseudoKatakana,
  Techno,
  CustomCharset,
};

struct Color {
  float r {};
  float g {};
  float b {};
  float a {};
};

struct Config {
  bool fullscreen = false;
  bool sway = false;
  int width = 1280;
  int height = 720;
  GlyphSet glyphSet = GlyphSet::Procedural;
  std::string customCharset;
  std::optional<std::uint32_t> seed;
  bool showHelp = false;
};

struct Layout {
  int width = 0;
  int height = 0;
  int cols = 0;
  int rows = 0;
  float pixelSize = 2.4f;
  float cellWidth = 14.0f;
  float cellHeight = 20.0f;
  float glyphOffsetX = 2.0f;
  float glyphOffsetY = 2.0f;
};

struct Column {
  float head = 0.0f;
  float speed = 18.0f;
  int trailLength = 14;
  float respawnGap = 6.0f;
  float phase = 0.0f;
  float intensity = 1.0f;
  float swayAmplitude = 0.0f;
  float swayFrequency = 0.0f;
  float swayDrift = 0.0f;
  float headPulse = 0.0f;
  float tint = 0.0f;
  float xOffset = 0.0f;
  std::vector<std::uint8_t> glyphs;
  std::vector<float> mutationTimers;
};

struct RainLayer {
  Layout layout;
  float glyphScale = 1.0f;
  float speedMultiplier = 1.0f;
  float trailMultiplier = 1.0f;
  float alphaMultiplier = 1.0f;
  float glowMultiplier = 1.0f;
  float veilMultiplier = 1.0f;
  float pulseMultiplier = 1.0f;
  float mutationMultiplier = 1.0f;
  float swayMultiplier = 1.0f;
  float horizontalJitter = 0.0f;
  float accentBias = 0.0f;
  std::vector<Column> columns;
};

struct AppState {
  Config config;
  Layout layout;
  std::vector<Glyph> glyphAtlas;
  std::array<RainLayer, 3> rainLayers;
  std::mt19937 rng;
  double timeSeconds = 0.0;
};

std::uint64_t splitmix64(std::uint64_t &state) {
  state += 0x9E3779B97F4A7C15ULL;
  std::uint64_t value = state;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

float randomFloat(std::mt19937 &rng, float minValue, float maxValue) {
  std::uniform_real_distribution<float> distribution(minValue, maxValue);
  return distribution(rng);
}

int randomInt(std::mt19937 &rng, int minValue, int maxValue) {
  std::uniform_int_distribution<int> distribution(minValue, maxValue);
  return distribution(rng);
}

std::uint8_t randomGlyphIndex(std::mt19937 &rng, std::size_t glyphCount) {
  return static_cast<std::uint8_t>(randomInt(rng, 0, static_cast<int>(glyphCount - 1)));
}

void setPixel(Glyph &glyph, int x, int y) {
  if (x < 0 || x >= kGlyphWidth || y < 0 || y >= kGlyphHeight) {
    return;
  }

  glyph[static_cast<std::size_t>(y)] |= static_cast<std::uint8_t>(1U << x);
}

int litPixelCount(const Glyph &glyph) {
  int total = 0;
  for (std::uint8_t row : glyph) {
    total += std::popcount(static_cast<unsigned int>(row));
  }
  return total;
}

void applySegment(Glyph &glyph, int segmentId) {
  switch (segmentId) {
    case 0:
      for (int x = 1; x <= 3; ++x) {
        setPixel(glyph, x, 0);
      }
      break;
    case 1:
      for (int x = 1; x <= 3; ++x) {
        setPixel(glyph, x, 2);
      }
      break;
    case 2:
      for (int x = 1; x <= 3; ++x) {
        setPixel(glyph, x, 4);
      }
      break;
    case 3:
      for (int x = 1; x <= 3; ++x) {
        setPixel(glyph, x, 6);
      }
      break;
    case 4:
      for (int y = 1; y <= 2; ++y) {
        setPixel(glyph, 0, y);
      }
      break;
    case 5:
      for (int y = 4; y <= 5; ++y) {
        setPixel(glyph, 0, y);
      }
      break;
    case 6:
      for (int y = 1; y <= 2; ++y) {
        setPixel(glyph, 4, y);
      }
      break;
    case 7:
      for (int y = 4; y <= 5; ++y) {
        setPixel(glyph, 4, y);
      }
      break;
    case 8:
      for (int y = 1; y <= 5; ++y) {
        setPixel(glyph, 2, y);
      }
      break;
    case 9:
      setPixel(glyph, 1, 1);
      setPixel(glyph, 2, 2);
      setPixel(glyph, 3, 3);
      setPixel(glyph, 4, 4);
      break;
    case 10:
      setPixel(glyph, 3, 1);
      setPixel(glyph, 2, 2);
      setPixel(glyph, 1, 3);
      setPixel(glyph, 0, 4);
      break;
    case 11:
      for (int y = 1; y <= 5; ++y) {
        setPixel(glyph, 1, y);
      }
      break;
    case 12:
      for (int y = 1; y <= 5; ++y) {
        setPixel(glyph, 3, y);
      }
      break;
    case 13:
      for (int x = 0; x < kGlyphWidth; ++x) {
        setPixel(glyph, x, 3);
      }
      break;
    case 14:
      setPixel(glyph, 0, 0);
      setPixel(glyph, 1, 1);
      setPixel(glyph, 2, 2);
      setPixel(glyph, 3, 3);
      setPixel(glyph, 4, 4);
      break;
    case 15:
      setPixel(glyph, 4, 0);
      setPixel(glyph, 3, 1);
      setPixel(glyph, 2, 2);
      setPixel(glyph, 1, 3);
      setPixel(glyph, 0, 4);
      break;
    case 16:
      setPixel(glyph, 0, 6);
      setPixel(glyph, 1, 5);
      setPixel(glyph, 2, 4);
      setPixel(glyph, 3, 3);
      setPixel(glyph, 4, 2);
      break;
    case 17:
      setPixel(glyph, 4, 6);
      setPixel(glyph, 3, 5);
      setPixel(glyph, 2, 4);
      setPixel(glyph, 1, 3);
      setPixel(glyph, 0, 2);
      break;
    default:
      break;
  }
}

Glyph buildProceduralGlyph(int glyphIndex) {
  Glyph glyph {};

  constexpr int segmentCount = 18;
  std::array<bool, segmentCount> used {};

  std::uint64_t state =
      0xD1B54A32D192ED03ULL ^ (static_cast<std::uint64_t>(glyphIndex + 1) * 0x9E3779B97F4A7C15ULL);

  const int structuralSegment = 8 + static_cast<int>(splitmix64(state) % 6ULL);
  applySegment(glyph, structuralSegment);
  used[static_cast<std::size_t>(structuralSegment)] = true;

  const int desiredSegments = 4 + static_cast<int>(splitmix64(state) % 4ULL);
  int placedSegments = 1;

  while (placedSegments < desiredSegments) {
    const int segment = static_cast<int>(splitmix64(state) % segmentCount);
    if (used[static_cast<std::size_t>(segment)]) {
      continue;
    }

    applySegment(glyph, segment);
    used[static_cast<std::size_t>(segment)] = true;
    ++placedSegments;
  }

  if ((splitmix64(state) & 1ULL) != 0) {
    setPixel(glyph, 0, 0);
  }
  if ((splitmix64(state) & 1ULL) != 0) {
    setPixel(glyph, 4, 6);
  }
  if ((splitmix64(state) & 1ULL) != 0) {
    setPixel(glyph, 2, 3);
  }

  if (litPixelCount(glyph) < 8) {
    applySegment(glyph, 13);
  }

  return glyph;
}

Glyph glyphFromPattern(const GlyphPattern &rows) {
  Glyph glyph {};

  for (int y = 0; y < kGlyphHeight; ++y) {
    const std::string_view row = rows[static_cast<std::size_t>(y)];
    for (int x = 0; x < kGlyphWidth && x < static_cast<int>(row.size()); ++x) {
      const char pixel = row[static_cast<std::size_t>(x)];
      if (pixel != '.' && pixel != ' ') {
        setPixel(glyph, x, y);
      }
    }
  }

  return glyph;
}

std::vector<Glyph> buildProceduralGlyphSet() {
  std::vector<Glyph> atlas;
  atlas.reserve(kGlyphCount);
  for (int index = 0; index < kGlyphCount; ++index) {
    atlas.push_back(buildProceduralGlyph(index));
  }
  return atlas;
}

std::vector<Glyph> buildPseudoKatakanaGlyphSet() {
  return {
      glyphFromPattern({"..#..", ".###.", "..#..", "..#..", ".#...", "#....", "....."}),
      glyphFromPattern({".###.", "...#.", "..#..", ".#...", "#....", ".....", "....."}),
      glyphFromPattern({"#####", "...#.", "..#..", ".#...", "#####", ".....", "....."}),
      glyphFromPattern({"#...#", ".#.#.", "..#..", "..#..", ".#...", "#....", "....."}),
      glyphFromPattern({".###.", "#...#", "....#", "..##.", ".#...", "#....", "....."}),
      glyphFromPattern({"####.", "...#.", "..#..", ".#...", "#....", "#....", "....."}),
      glyphFromPattern({"#####", "..#..", "..#..", "..#..", ".#...", "#....", "....."}),
      glyphFromPattern({"#...#", "#...#", ".###.", "...#.", "..#..", ".#...", "#...."}),
      glyphFromPattern({".####", "#....", "#....", ".###.", "...#.", "..#..", ".#..."}),
      glyphFromPattern({"#....", "##...", "#.#..", "#..#.", "#...#", ".###.", "....."}),
      glyphFromPattern({".###.", "#....", "#....", "#....", "#..#.", ".##..", "....."}),
      glyphFromPattern({"#####", "#....", "###..", "#....", "#....", "#####", "....."}),
      glyphFromPattern({"#...#", "##.##", "#.#.#", "#...#", ".###.", "..#..", "....."}),
      glyphFromPattern({"###..", "#..#.", "###..", "#..#.", "#..#.", "###..", "....."}),
      glyphFromPattern({"..#..", ".#.#.", "#...#", "#####", "...#.", "..#..", ".#..."}),
      glyphFromPattern({".###.", "#...#", "...#.", "..#..", ".#...", "#...#", ".###."}),
      glyphFromPattern({"#####", "#...#", "...#.", "..#..", ".#...", "#....", "#####"}),
      glyphFromPattern({"#..#.", "#..#.", ".##..", "..#..", ".##..", "#..#.", "#..#."}),
      glyphFromPattern({".#.#.", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", ".#.#."}),
      glyphFromPattern({"##..#", "..#..", ".###.", "#...#", ".###.", "..#..", "#..##"}),
  };
}

std::vector<Glyph> buildTechnoGlyphSet() {
  return {
      glyphFromPattern({".###.", "#...#", "#.#.#", "#.#.#", "#...#", ".###.", "....."}),
      glyphFromPattern({"#####", "#.#.#", "..#..", "#.#.#", "#####", ".....", "....."}),
      glyphFromPattern({"#...#", ".#.#.", "..#..", ".#.#.", "#...#", ".....", "....."}),
      glyphFromPattern({"###..", "#..#.", "###..", "#..#.", "#..#.", "###..", "....."}),
      glyphFromPattern({"#####", "#....", "####.", "#....", "#....", "#####", "....."}),
      glyphFromPattern({".####", "#....", "#.###", "#...#", "#...#", ".###.", "....."}),
      glyphFromPattern({"##.##", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "##.##"}),
      glyphFromPattern({"#...#", "##..#", "#.#.#", "#..##", "#...#", ".....", "....."}),
      glyphFromPattern({"..#..", ".###.", "#####", ".###.", "..#..", ".....", "....."}),
      glyphFromPattern({"#.#.#", "#####", ".###.", "..#..", ".###.", "#####", "#.#.#"}),
      glyphFromPattern({"###.#", "#..#.", "..#..", ".#...", "#..#.", "###.#", "....."}),
      glyphFromPattern({"#....", ".#...", "..#..", "...#.", "....#", "...#.", "..#.."}),
      glyphFromPattern({"..#..", ".##..", "#####", ".##..", "..#..", ".##..", "#####"}),
      glyphFromPattern({"#####", "##.##", "#.#.#", "..#..", "#.#.#", "##.##", "#####"}),
      glyphFromPattern({"##..#", ".#.#.", "..#..", ".#.#.", "#..##", ".....", "....."}),
      glyphFromPattern({".###.", "#.#.#", "##.##", "#...#", "##.##", "#.#.#", ".###."}),
      glyphFromPattern({"#####", "...#.", "..#..", ".#...", "#....", "#####", "....."}),
      glyphFromPattern({".#.#.", "#.#.#", ".#.#.", "#####", ".#.#.", "#.#.#", ".#.#."}),
      glyphFromPattern({"#####", "#...#", "#.#.#", "..#..", "#.#.#", "#...#", "#####"}),
      glyphFromPattern({".###.", "#...#", "..#..", ".###.", "..#..", "#...#", ".###."}),
  };
}

std::optional<Glyph> tryBuildAsciiGlyph(char symbol) {
  switch (symbol) {
    case 'A':
      return glyphFromPattern({".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"});
    case 'B':
      return glyphFromPattern({"####.", "#...#", "####.", "#...#", "#...#", "#...#", "####."});
    case 'C':
      return glyphFromPattern({".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."});
    case 'D':
      return glyphFromPattern({"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."});
    case 'E':
      return glyphFromPattern({"#####", "#....", "####.", "#....", "#....", "#....", "#####"});
    case 'F':
      return glyphFromPattern({"#####", "#....", "####.", "#....", "#....", "#....", "#...."});
    case 'G':
      return glyphFromPattern({".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."});
    case 'H':
      return glyphFromPattern({"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"});
    case 'I':
      return glyphFromPattern({"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"});
    case 'J':
      return glyphFromPattern({"..###", "...#.", "...#.", "...#.", "#..#.", "#..#.", ".##.."});
    case 'K':
      return glyphFromPattern({"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"});
    case 'L':
      return glyphFromPattern({"#....", "#....", "#....", "#....", "#....", "#....", "#####"});
    case 'M':
      return glyphFromPattern({"#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"});
    case 'N':
      return glyphFromPattern({"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"});
    case 'O':
      return glyphFromPattern({".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."});
    case 'P':
      return glyphFromPattern({"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."});
    case 'Q':
      return glyphFromPattern({".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"});
    case 'R':
      return glyphFromPattern({"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"});
    case 'S':
      return glyphFromPattern({".####", "#....", "#....", ".###.", "....#", "....#", "####."});
    case 'T':
      return glyphFromPattern({"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."});
    case 'U':
      return glyphFromPattern({"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."});
    case 'V':
      return glyphFromPattern({"#...#", "#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."});
    case 'W':
      return glyphFromPattern({"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"});
    case 'X':
      return glyphFromPattern({"#...#", ".#.#.", "..#..", "..#..", "..#..", ".#.#.", "#...#"});
    case 'Y':
      return glyphFromPattern({"#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..", "..#.."});
    case 'Z':
      return glyphFromPattern({"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"});
    case '0':
      return glyphFromPattern({".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."});
    case '1':
      return glyphFromPattern({"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."});
    case '2':
      return glyphFromPattern({".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"});
    case '3':
      return glyphFromPattern({"####.", "....#", "...#.", "..##.", "....#", "#...#", ".###."});
    case '4':
      return glyphFromPattern({"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."});
    case '5':
      return glyphFromPattern({"#####", "#....", "####.", "....#", "....#", "#...#", ".###."});
    case '6':
      return glyphFromPattern({".###.", "#...#", "#....", "####.", "#...#", "#...#", ".###."});
    case '7':
      return glyphFromPattern({"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."});
    case '8':
      return glyphFromPattern({".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."});
    case '9':
      return glyphFromPattern({".###.", "#...#", "#...#", ".####", "....#", "#...#", ".###."});
    case '#':
      return glyphFromPattern({".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".....", "....."});
    case '@':
      return glyphFromPattern({".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".####"});
    case '%':
      return glyphFromPattern({"##..#", "##.#.", "...#.", "..#..", ".#...", "#.##.", "#..##"});
    case '+':
      return glyphFromPattern({".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."});
    case '-':
      return glyphFromPattern({".....", ".....", "#####", ".....", ".....", ".....", "....."});
    case '*':
      return glyphFromPattern({".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."});
    case '/':
      return glyphFromPattern({"....#", "...#.", "..#..", ".#...", "#....", ".....", "....."});
    case '\\':
      return glyphFromPattern({"#....", ".#...", "..#..", "...#.", "....#", ".....", "....."});
    case '!':
      return glyphFromPattern({"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."});
    case '?':
      return glyphFromPattern({".###.", "#...#", "...#.", "..#..", "..#..", ".....", "..#.."});
    case '<':
      return glyphFromPattern({"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."});
    case '>':
      return glyphFromPattern({".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."});
    case '[':
      return glyphFromPattern({".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."});
    case ']':
      return glyphFromPattern({".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."});
    case '(':
      return glyphFromPattern({"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."});
    case ')':
      return glyphFromPattern({".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."});
    case '|':
      return glyphFromPattern({"..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."});
    case '_':
      return glyphFromPattern({".....", ".....", ".....", ".....", ".....", ".....", "#####"});
    case '=':
      return glyphFromPattern({".....", "#####", ".....", "#####", ".....", ".....", "....."});
    case ':':
      return glyphFromPattern({".....", "..#..", ".....", ".....", "..#..", ".....", "....."});
    case '.':
      return glyphFromPattern({".....", ".....", ".....", ".....", ".....", ".....", "..#.."});
    default:
      return std::nullopt;
  }
}

char normalizeCharsetSymbol(unsigned char symbol) {
  if (std::isalpha(symbol) != 0) {
    return static_cast<char>(std::toupper(symbol));
  }

  return static_cast<char>(symbol);
}

std::vector<Glyph> buildCustomCharsetGlyphSet(std::string_view charset) {
  std::vector<Glyph> atlas;
  std::string uniqueSymbols;

  for (unsigned char rawSymbol : charset) {
    if (std::isspace(rawSymbol) != 0) {
      continue;
    }

    const char symbol = normalizeCharsetSymbol(rawSymbol);
    if (uniqueSymbols.find(symbol) != std::string::npos) {
      continue;
    }

    uniqueSymbols.push_back(symbol);
    if (const auto glyph = tryBuildAsciiGlyph(symbol); glyph.has_value()) {
      atlas.push_back(*glyph);
    } else {
      atlas.push_back(buildProceduralGlyph(static_cast<int>(rawSymbol) + (static_cast<int>(atlas.size()) * 17)));
    }
  }

  if (atlas.empty()) {
    return buildProceduralGlyphSet();
  }

  return atlas;
}

std::optional<GlyphSet> parseGlyphSetName(std::string_view text) {
  std::string normalized;
  normalized.reserve(text.size());

  for (unsigned char symbol : text) {
    normalized.push_back(static_cast<char>(std::tolower(symbol)));
  }

  if (normalized == "procedural" || normalized == "abstract") {
    return GlyphSet::Procedural;
  }
  if (normalized == "pseudo-katakana" || normalized == "pseudo_katakana" || normalized == "katakana") {
    return GlyphSet::PseudoKatakana;
  }
  if (normalized == "techno" || normalized == "techno-symbols" || normalized == "techno_symbols") {
    return GlyphSet::Techno;
  }
  if (normalized == "custom" || normalized == "charset") {
    return GlyphSet::CustomCharset;
  }

  return std::nullopt;
}

bool hasVisibleCharsetSymbols(std::string_view text) {
  for (unsigned char symbol : text) {
    if (std::isspace(symbol) == 0) {
      return true;
    }
  }

  return false;
}

std::vector<Glyph> buildGlyphAtlas(const Config &config) {
  switch (config.glyphSet) {
    case GlyphSet::Procedural:
      return buildProceduralGlyphSet();
    case GlyphSet::PseudoKatakana:
      return buildPseudoKatakanaGlyphSet();
    case GlyphSet::Techno:
      return buildTechnoGlyphSet();
    case GlyphSet::CustomCharset:
      return buildCustomCharsetGlyphSet(config.customCharset);
  }

  return buildProceduralGlyphSet();
}

float clampDelta(double value) {
  return std::clamp(static_cast<float>(value), 0.0f, 0.05f);
}

float saturate(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value) {
  if (std::abs(edge1 - edge0) < 1.0e-6f) {
    return value < edge0 ? 0.0f : 1.0f;
  }

  const float t = saturate((value - edge0) / (edge1 - edge0));
  return t * t * (3.0f - (2.0f * t));
}

Color withAlpha(Color color, float alpha) {
  color.a = alpha;
  return color;
}

Color scaleAlpha(Color color, float scale) {
  color.a *= scale;
  return color;
}

Color mixColor(const Color &from, const Color &to, float t) {
  const float factor = saturate(t);
  return {
      std::lerp(from.r, to.r, factor),
      std::lerp(from.g, to.g, factor),
      std::lerp(from.b, to.b, factor),
      std::lerp(from.a, to.a, factor),
  };
}

std::array<RainLayer, 3> createRainLayers() {
  return {{
      {
          .layout = {},
          .glyphScale = 0.72f,
          .speedMultiplier = 0.56f,
          .trailMultiplier = 0.72f,
          .alphaMultiplier = 0.30f,
          .glowMultiplier = 0.28f,
          .veilMultiplier = 0.22f,
          .pulseMultiplier = 0.55f,
          .mutationMultiplier = 0.76f,
          .swayMultiplier = 0.60f,
          .horizontalJitter = 0.16f,
          .accentBias = 0.14f,
          .columns = {},
      },
      {
          .layout = {},
          .glyphScale = 1.0f,
          .speedMultiplier = 1.0f,
          .trailMultiplier = 1.0f,
          .alphaMultiplier = 0.72f,
          .glowMultiplier = 0.72f,
          .veilMultiplier = 0.60f,
          .pulseMultiplier = 0.95f,
          .mutationMultiplier = 1.0f,
          .swayMultiplier = 1.0f,
          .horizontalJitter = 0.08f,
          .accentBias = 0.0f,
          .columns = {},
      },
      {
          .layout = {},
          .glyphScale = 1.28f,
          .speedMultiplier = 1.40f,
          .trailMultiplier = 1.20f,
          .alphaMultiplier = 0.96f,
          .glowMultiplier = 1.12f,
          .veilMultiplier = 1.0f,
          .pulseMultiplier = 1.28f,
          .mutationMultiplier = 1.18f,
          .swayMultiplier = 1.18f,
          .horizontalJitter = 0.12f,
          .accentBias = -0.10f,
          .columns = {},
      },
  }};
}

Layout buildLayerLayout(const Layout &baseLayout, float glyphScale) {
  Layout layout;
  layout.width = baseLayout.width;
  layout.height = baseLayout.height;

  if (baseLayout.width <= 0 || baseLayout.height <= 0) {
    return layout;
  }

  layout.pixelSize = std::max(1.15f, baseLayout.pixelSize * glyphScale);
  layout.cellWidth = std::max(8.0f, std::round(layout.pixelSize * 6.0f));
  layout.cellHeight = std::max(12.0f, std::round(layout.pixelSize * 9.0f));
  layout.cols = std::max(1, static_cast<int>(std::ceil(static_cast<float>(layout.width) / layout.cellWidth)));
  layout.rows = std::max(1, static_cast<int>(std::ceil(static_cast<float>(layout.height) / layout.cellHeight)));
  layout.glyphOffsetX = std::round((layout.cellWidth - (layout.pixelSize * kGlyphWidth)) * 0.5f);
  layout.glyphOffsetY = std::round((layout.cellHeight - (layout.pixelSize * kGlyphHeight)) * 0.5f);
  return layout;
}

void updateLayout(AppState &app, int width, int height) {
  Layout layout;
  layout.width = width;
  layout.height = height;

  if (width <= 0 || height <= 0) {
    app.layout = layout;
    return;
  }

  const float reference = static_cast<float>(std::min(width, height));
  layout.pixelSize = std::clamp(reference / 320.0f, 1.6f, 4.0f);
  layout.cellWidth = std::round(layout.pixelSize * 6.0f);
  layout.cellHeight = std::round(layout.pixelSize * 9.0f);
  layout.cols = std::max(1, static_cast<int>(std::ceil(static_cast<float>(width) / layout.cellWidth)));
  layout.rows = std::max(1, static_cast<int>(std::ceil(static_cast<float>(height) / layout.cellHeight)));
  layout.glyphOffsetX = std::round((layout.cellWidth - (layout.pixelSize * kGlyphWidth)) * 0.5f);
  layout.glyphOffsetY = std::round((layout.cellHeight - (layout.pixelSize * kGlyphHeight)) * 0.5f);

  app.layout = layout;
}

void resetColumn(AppState &app, RainLayer &layer, Column &column, bool staggeredStart) {
  const int rowCount = layer.layout.rows;
  const int baseMinTrail = std::max(8, rowCount / 7);
  const int baseMaxTrail = std::max(baseMinTrail + 2, rowCount / 2);
  const int minTrail = std::max(6, static_cast<int>(std::round(static_cast<float>(baseMinTrail) * layer.trailMultiplier)));
  const int maxTrail =
      std::max(minTrail + 2, static_cast<int>(std::round(static_cast<float>(baseMaxTrail) * layer.trailMultiplier)));

  column.speed = randomFloat(app.rng, 10.0f, 28.0f) * layer.speedMultiplier;
  column.trailLength = randomInt(app.rng, minTrail, maxTrail);
  column.respawnGap = randomFloat(app.rng, 2.0f, 14.0f) * std::lerp(0.85f, 1.15f, layer.glyphScale - 0.5f);
  column.phase = randomFloat(app.rng, 0.0f, 2.0f * kPi);
  column.intensity = randomFloat(app.rng, 0.85f, 1.2f) * std::lerp(0.92f, 1.08f, saturate(layer.alphaMultiplier));
  column.swayAmplitude = randomFloat(app.rng, 0.12f, 0.75f) * layer.swayMultiplier;
  column.swayFrequency = randomFloat(app.rng, 0.45f, 1.35f);
  column.swayDrift = randomFloat(app.rng, 0.10f, 0.28f);
  column.headPulse = randomFloat(app.rng, 0.0f, 0.65f) * layer.pulseMultiplier;
  column.tint = randomFloat(app.rng, 0.0f, 1.0f);
  column.xOffset = randomFloat(app.rng,
                               -layer.layout.cellWidth * layer.horizontalJitter,
                               layer.layout.cellWidth * layer.horizontalJitter);
  column.head = staggeredStart
                    ? randomFloat(app.rng, -static_cast<float>(rowCount), static_cast<float>(rowCount))
                    : randomFloat(app.rng, -static_cast<float>(column.trailLength) * 1.5f, -2.0f);

  column.glyphs.assign(static_cast<std::size_t>(rowCount), 0);
  column.mutationTimers.assign(static_cast<std::size_t>(rowCount), 0.0f);

  for (int row = 0; row < rowCount; ++row) {
    column.glyphs[static_cast<std::size_t>(row)] = randomGlyphIndex(app.rng, app.glyphAtlas.size());
    column.mutationTimers[static_cast<std::size_t>(row)] = randomFloat(app.rng, 0.03f, 0.35f);
  }
}

void rebuildColumns(AppState &app) {
  if (app.layout.width <= 0 || app.layout.height <= 0) {
    for (RainLayer &layer : app.rainLayers) {
      layer.layout = {};
      layer.columns.clear();
    }
    return;
  }

  for (RainLayer &layer : app.rainLayers) {
    layer.layout = buildLayerLayout(app.layout, layer.glyphScale);
    layer.columns.assign(static_cast<std::size_t>(layer.layout.cols), {});
    for (Column &column : layer.columns) {
      resetColumn(app, layer, column, true);
    }
  }
}

void updateColumns(AppState &app, float deltaSeconds) {
  for (RainLayer &layer : app.rainLayers) {
    if (layer.columns.empty()) {
      continue;
    }

    const int rowCount = layer.layout.rows;

    for (Column &column : layer.columns) {
      column.head += column.speed * deltaSeconds;
      column.headPulse =
          std::max(0.0f, column.headPulse - (deltaSeconds * (1.05f / std::max(0.45f, layer.pulseMultiplier))));

      const int minRow = std::max(0, static_cast<int>(std::floor(column.head)) - column.trailLength - 1);
      const int maxRow = std::min(rowCount - 1, static_cast<int>(std::ceil(column.head)) + 1);

      for (int row = minRow; row <= maxRow; ++row) {
        auto &timer = column.mutationTimers[static_cast<std::size_t>(row)];
        timer -= deltaSeconds;

        if (timer <= 0.0f) {
          column.glyphs[static_cast<std::size_t>(row)] = randomGlyphIndex(app.rng, app.glyphAtlas.size());
          timer = randomFloat(app.rng, 0.04f, 0.28f) / layer.mutationMultiplier;
        }
      }

      if (randomFloat(app.rng, 0.0f, 1.0f) < deltaSeconds * 8.0f * layer.mutationMultiplier) {
        const int row = randomInt(app.rng, 0, rowCount - 1);
        column.glyphs[static_cast<std::size_t>(row)] = randomGlyphIndex(app.rng, app.glyphAtlas.size());
        column.mutationTimers[static_cast<std::size_t>(row)] = randomFloat(app.rng, 0.04f, 0.24f) / layer.mutationMultiplier;
      }

      if (randomFloat(app.rng, 0.0f, 1.0f) <
          deltaSeconds * (1.5f + (column.intensity * 1.4f)) * layer.pulseMultiplier) {
        column.headPulse = std::max(column.headPulse, randomFloat(app.rng, 0.4f, 1.0f) * layer.pulseMultiplier);
      }

      if (column.head - static_cast<float>(column.trailLength) >
          static_cast<float>(rowCount) + column.respawnGap) {
        resetColumn(app, layer, column, false);
      }
    }
  }
}

void emitRect(float x, float y, float width, float height, const Color &color) {
  glColor4f(color.r, color.g, color.b, color.a);
  glVertex2f(x, y);
  glVertex2f(x + width, y);
  glVertex2f(x + width, y + height);
  glVertex2f(x, y + height);
}

void emitGradientRect(float x,
                      float y,
                      float width,
                      float height,
                      const Color &topLeft,
                      const Color &topRight,
                      const Color &bottomRight,
                      const Color &bottomLeft) {
  glColor4f(topLeft.r, topLeft.g, topLeft.b, topLeft.a);
  glVertex2f(x, y);
  glColor4f(topRight.r, topRight.g, topRight.b, topRight.a);
  glVertex2f(x + width, y);
  glColor4f(bottomRight.r, bottomRight.g, bottomRight.b, bottomRight.a);
  glVertex2f(x + width, y + height);
  glColor4f(bottomLeft.r, bottomLeft.g, bottomLeft.b, bottomLeft.a);
  glVertex2f(x, y + height);
}

void emitVerticalGradientRect(float x,
                              float y,
                              float width,
                              float height,
                              const Color &topColor,
                              const Color &bottomColor) {
  emitGradientRect(x, y, width, height, topColor, topColor, bottomColor, bottomColor);
}

void emitHorizontalGradientRect(float x,
                                float y,
                                float width,
                                float height,
                                const Color &leftColor,
                                const Color &rightColor) {
  emitGradientRect(x, y, width, height, leftColor, rightColor, rightColor, leftColor);
}

void drawSoftEllipse(float centerX,
                     float centerY,
                     float radiusX,
                     float radiusY,
                     const Color &innerColor,
                     const Color &outerColor,
                     int segments = 36) {
  glBegin(GL_TRIANGLE_FAN);
  glColor4f(innerColor.r, innerColor.g, innerColor.b, innerColor.a);
  glVertex2f(centerX, centerY);

  for (int segment = 0; segment <= segments; ++segment) {
    const float angle = (static_cast<float>(segment) / static_cast<float>(segments)) * (2.0f * kPi);
    glColor4f(outerColor.r, outerColor.g, outerColor.b, outerColor.a);
    glVertex2f(centerX + (std::cos(angle) * radiusX), centerY + (std::sin(angle) * radiusY));
  }

  glEnd();
}

void emitGlyph(const Glyph &glyph,
               float originX,
               float originY,
               float pixelSize,
               const Color &color,
               float expansion) {
  for (int y = 0; y < kGlyphHeight; ++y) {
    const std::uint8_t rowBits = glyph[static_cast<std::size_t>(y)];
    for (int x = 0; x < kGlyphWidth; ++x) {
      if ((rowBits & static_cast<std::uint8_t>(1U << x)) == 0) {
        continue;
      }

      const float inset = expansion * 0.5f;
      emitRect(originX + (static_cast<float>(x) * pixelSize) - inset,
               originY + (static_cast<float>(y) * pixelSize) - inset,
               pixelSize + expansion,
               pixelSize + expansion,
               color);
    }
  }
}

void configureProjection(const Layout &layout) {
  glViewport(0, 0, layout.width, layout.height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(layout.width), static_cast<double>(layout.height), 0.0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

float computeColumnOffset(const AppState &app, const RainLayer &layer, const Column &column, float rowPosition) {
  if (!app.config.sway) {
    return column.xOffset;
  }

  const float primary =
      std::sin((static_cast<float>(app.timeSeconds) * column.swayFrequency) + column.phase +
               (rowPosition * column.swayDrift));
  const float secondary =
      std::sin((static_cast<float>(app.timeSeconds) * (column.swayFrequency * 0.42f)) + (column.phase * 1.7f) -
               (rowPosition * (column.swayDrift * 0.65f)));
  return column.xOffset + (((primary * 0.74f) + (secondary * 0.26f)) * column.swayAmplitude * layer.layout.cellWidth);
}

void drawBackground(const AppState &app) {
  const Layout &layout = app.layout;
  const float width = static_cast<float>(layout.width);
  const float height = static_cast<float>(layout.height);
  const float time = static_cast<float>(app.timeSeconds);

  glBegin(GL_QUADS);
  emitVerticalGradientRect(
      0.0f, 0.0f, width, height, {0.0f, 0.085f, 0.028f, 1.0f}, {0.0f, 0.012f, 0.002f, 1.0f});
  emitHorizontalGradientRect(
      0.0f, 0.0f, width * 0.55f, height, {0.0f, 0.14f, 0.045f, 0.08f}, {0.0f, 0.0f, 0.0f, 0.0f});
  emitHorizontalGradientRect(
      width * 0.35f, 0.0f, width * 0.65f, height, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.10f, 0.035f, 0.06f});
  glEnd();

  drawSoftEllipse(width * (0.18f + (0.03f * std::sin(time * 0.17f))),
                  height * 0.16f,
                  width * 0.34f,
                  height * 0.18f,
                  {0.01f, 0.16f, 0.07f, 0.08f},
                  {0.0f, 0.15f, 0.05f, 0.0f});
  drawSoftEllipse(width * (0.78f + (0.04f * std::sin((time * 0.11f) + 1.8f))),
                  height * 0.26f,
                  width * 0.26f,
                  height * 0.22f,
                  {0.0f, 0.10f, 0.05f, 0.05f},
                  {0.0f, 0.10f, 0.03f, 0.0f});
  drawSoftEllipse(width * 0.50f,
                  height * (0.92f + (0.01f * std::sin(time * 0.09f))),
                  width * 0.48f,
                  height * 0.18f,
                  {0.0f, 0.08f, 0.03f, 0.05f},
                  {0.0f, 0.07f, 0.02f, 0.0f});
}

Color makeCoreColor(const RainLayer &layer, const Column &column, float intensity, float shimmer, bool isHead) {
  const float accent = saturate(column.tint + layer.accentBias);

  if (isHead) {
    return {
        std::clamp(0.78f + (accent * 0.08f), 0.0f, 1.0f),
        1.0f,
        std::clamp(0.88f + (column.headPulse * 0.08f) + (accent * 0.08f), 0.0f, 1.0f),
        std::clamp((0.92f + (column.headPulse * 0.10f)) * std::lerp(0.8f, 1.0f, layer.alphaMultiplier), 0.0f, 1.0f),
    };
  }

  const float alpha =
      std::clamp((0.10f + (intensity * 0.82f) + (column.headPulse * 0.10f)) * column.intensity * layer.alphaMultiplier,
                 0.0f,
                 0.94f);
  const Color emeraldBase = {
      std::clamp(0.010f + (intensity * 0.05f), 0.0f, 0.24f),
      std::clamp(0.24f + (intensity * 0.72f) + (shimmer * 0.08f), 0.0f, 1.0f),
      std::clamp(0.03f + (intensity * 0.08f), 0.0f, 0.22f),
      alpha,
  };
  const Color coolVariant = {
      std::clamp(0.02f + (intensity * 0.07f), 0.0f, 0.30f),
      std::clamp(0.28f + (intensity * 0.68f) + (accent * 0.04f), 0.0f, 1.0f),
      std::clamp(0.05f + (intensity * 0.19f) + (column.headPulse * 0.03f), 0.0f, 0.34f),
      alpha,
  };
  return withAlpha(mixColor(emeraldBase, coolVariant, (accent * 0.55f) + (column.headPulse * 0.15f)), alpha);
}

Color makeGlowColor(const RainLayer &layer, const Column &column, float intensity, bool isHead) {
  const float accent = saturate(column.tint + layer.accentBias);

  if (isHead) {
    return scaleAlpha(Color {
        std::clamp(0.12f + (accent * 0.05f), 0.0f, 0.4f),
        0.95f,
        std::clamp(0.16f + (accent * 0.12f), 0.0f, 0.4f),
        std::clamp(0.22f + (column.headPulse * 0.22f), 0.0f, 0.5f),
    }, (1.0f + (column.headPulse * 0.2f)) * layer.glowMultiplier);
  }

  return scaleAlpha({
      0.0f,
      std::clamp(0.08f + (intensity * 0.28f) + (accent * 0.04f), 0.0f, 0.5f),
      std::clamp(0.015f + (accent * 0.03f), 0.0f, 0.12f),
      intensity * (0.08f + (column.headPulse * 0.06f)),
  }, layer.glowMultiplier);
}

void drawColumnVeils(const AppState &app, const RainLayer &layer) {
  const Layout &layout = layer.layout;

  if (layer.columns.empty()) {
    return;
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBegin(GL_QUADS);

  for (std::size_t columnIndex = 0; columnIndex < layer.columns.size(); ++columnIndex) {
    const Column &column = layer.columns[columnIndex];
    const float trailTopRow = std::max(0.0f, column.head - static_cast<float>(column.trailLength));
    const float trailBottomRow = std::min(static_cast<float>(layout.rows), column.head + 1.35f);

    if (trailBottomRow <= 0.0f || trailTopRow >= static_cast<float>(layout.rows)) {
      continue;
    }

    const float headOffset = computeColumnOffset(app, layer, column, column.head);
    const float centerX = (static_cast<float>(columnIndex) * layout.cellWidth) + layout.glyphOffsetX +
                          (layout.pixelSize * (kGlyphWidth * 0.5f)) + headOffset;
    const float topY = (trailTopRow * layout.cellHeight) + layout.glyphOffsetY;
    const float bottomY = std::min(static_cast<float>(layout.height),
                                   (trailBottomRow * layout.cellHeight) + layout.glyphOffsetY);
    const float height = std::max(layout.pixelSize * 2.0f, bottomY - topY);

    const float outerWidth = layout.cellWidth * (1.15f + (column.tint * 0.25f));
    const float innerWidth = layout.cellWidth * (0.55f + (column.tint * 0.12f));

    const Color outerTop = {0.0f, 0.16f, 0.04f, 0.0f};
    const Color outerBottom = {0.01f, 0.34f + (column.tint * 0.08f), 0.06f + (column.tint * 0.04f),
                               (0.04f + (column.headPulse * 0.09f)) * layer.veilMultiplier};
    const Color innerTop = {0.02f, 0.26f, 0.08f, 0.0f};
    const Color innerBottom = {0.08f,
                               0.80f,
                               0.18f + (column.tint * 0.08f),
                               (0.03f + (column.headPulse * 0.10f)) * layer.veilMultiplier};

    emitVerticalGradientRect(centerX - (outerWidth * 0.5f), topY, outerWidth, height, outerTop, outerBottom);
    emitVerticalGradientRect(centerX - (innerWidth * 0.5f),
                             topY,
                             innerWidth,
                             height,
                             innerTop,
                             scaleAlpha(innerBottom, 1.0f + (column.headPulse * 0.35f)));
  }

  glEnd();

  for (std::size_t columnIndex = 0; columnIndex < layer.columns.size(); ++columnIndex) {
    const Column &column = layer.columns[columnIndex];
    if (column.head < -1.0f || column.head > static_cast<float>(layout.rows) + 1.0f) {
      continue;
    }

    const float headOffset = computeColumnOffset(app, layer, column, column.head);
    const float headX = (static_cast<float>(columnIndex) * layout.cellWidth) + layout.glyphOffsetX +
                        (layout.pixelSize * (kGlyphWidth * 0.5f)) + headOffset;
    const float headY = (column.head * layout.cellHeight) + layout.glyphOffsetY + (layout.pixelSize * 3.5f);
    const float pulseScale = 1.0f + (column.headPulse * 0.45f * layer.pulseMultiplier);

    drawSoftEllipse(headX,
                    headY,
                    layout.cellWidth * 0.95f * pulseScale,
                    layout.cellHeight * 0.85f * pulseScale,
                    {0.12f,
                     0.92f,
                     0.22f + (column.tint * 0.08f),
                     (0.10f + (column.headPulse * 0.12f)) * layer.veilMultiplier},
                    {0.0f, 0.72f, 0.15f, 0.0f},
                    28);
    drawSoftEllipse(headX,
                    headY,
                    layout.cellWidth * 1.85f * pulseScale,
                    layout.cellHeight * 1.45f * pulseScale,
                    {0.02f, 0.35f, 0.09f, (0.03f + (column.headPulse * 0.06f)) * layer.veilMultiplier},
                    {0.0f, 0.24f, 0.05f, 0.0f},
                    28);
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void drawRainGlyphs(const AppState &app, const RainLayer &layer, bool glowPass) {
  if (layer.columns.empty()) {
    return;
  }

  glBegin(GL_QUADS);

  for (std::size_t columnIndex = 0; columnIndex < layer.columns.size(); ++columnIndex) {
    const Column &column = layer.columns[columnIndex];
    const int startRow = std::max(0, static_cast<int>(std::floor(column.head)) - column.trailLength);
    const int endRow = std::min(layer.layout.rows - 1, static_cast<int>(std::ceil(column.head)));

    for (int row = startRow; row <= endRow; ++row) {
      const float distanceFromHead = column.head - static_cast<float>(row);
      if (distanceFromHead < 0.0f || distanceFromHead > static_cast<float>(column.trailLength)) {
        continue;
      }

      const bool isHead = distanceFromHead < 0.85f;
      const float normalized = 1.0f - (distanceFromHead / static_cast<float>(column.trailLength));
      const float intensity = smoothstep(0.0f, 1.0f, std::pow(std::max(normalized, 0.0f), 1.35f));
      const float shimmer =
          std::sin(static_cast<float>(app.timeSeconds) * 7.5f + column.phase + (static_cast<float>(row) * 0.55f));

      const float offset = computeColumnOffset(app, layer, column, static_cast<float>(row));
      const float x = (static_cast<float>(columnIndex) * layer.layout.cellWidth) + layer.layout.glyphOffsetX + offset;
      const float y = (static_cast<float>(row) * layer.layout.cellHeight) + layer.layout.glyphOffsetY;
      const Glyph &glyph = app.glyphAtlas[column.glyphs[static_cast<std::size_t>(row)]];

      if (glowPass) {
        const Color glowColor = makeGlowColor(layer, column, intensity, isHead);
        const float glowExpansion =
            layer.layout.pixelSize *
            (0.65f + (intensity * 0.40f) + (isHead ? (0.30f + (column.headPulse * 0.25f)) : 0.0f)) *
            std::lerp(0.92f, 1.08f, layer.glowMultiplier * 0.5f);
        emitGlyph(glyph, x, y, layer.layout.pixelSize, glowColor, glowExpansion);
      } else {
        const Color coreColor = makeCoreColor(layer, column, intensity, shimmer, isHead);
        emitGlyph(glyph, x, y, layer.layout.pixelSize, coreColor, 0.0f);
      }
    }
  }

  glEnd();
}

void drawForegroundEffects(const AppState &app) {
  const Layout &layout = app.layout;
  const float width = static_cast<float>(layout.width);
  const float height = static_cast<float>(layout.height);
  const float time = static_cast<float>(app.timeSeconds);

  glBegin(GL_QUADS);

  const float scanlineStep = std::max(3.0f, layout.pixelSize * 3.5f);
  for (float y = 0.0f; y < height; y += scanlineStep) {
    const float flicker = 0.5f + (0.5f * std::sin((time * 0.8f) + (y * 0.035f)));
    emitRect(0.0f, y, width, 1.0f, {0.0f, 0.0f, 0.0f, 0.018f + (flicker * 0.018f)});
  }

  const float stripeStep = std::max(layout.cellWidth * 1.15f, 9.0f);
  for (float x = 0.0f; x < width; x += stripeStep) {
    const float pulse = 0.5f + (0.5f * std::sin((time * 0.45f) + (x * 0.014f)));
    emitRect(x, 0.0f, std::max(1.0f, layout.pixelSize * 0.35f), height, {0.03f, 0.14f, 0.05f, 0.004f + (pulse * 0.008f)});
  }

  const float topShade = height * 0.14f;
  const float bottomShade = height * 0.20f;
  const float sideShade = width * 0.18f;

  emitVerticalGradientRect(0.0f, 0.0f, width, topShade, {0.0f, 0.0f, 0.0f, 0.16f}, {0.0f, 0.0f, 0.0f, 0.0f});
  emitVerticalGradientRect(0.0f, height - bottomShade, width, bottomShade, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.26f});
  emitHorizontalGradientRect(0.0f, 0.0f, sideShade, height, {0.0f, 0.0f, 0.0f, 0.24f}, {0.0f, 0.0f, 0.0f, 0.0f});
  emitHorizontalGradientRect(width - sideShade, 0.0f, sideShade, height, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.24f});

  const float centerSheen = 0.5f + (0.5f * std::sin((time * 0.25f) + 0.8f));
  emitVerticalGradientRect(width * 0.32f,
                           0.0f,
                           width * 0.36f,
                           height,
                           {0.02f, 0.16f, 0.05f, 0.014f + (centerSheen * 0.008f)},
                           {0.0f, 0.02f, 0.01f, 0.0f});
  glEnd();

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  drawSoftEllipse(width * 0.50f,
                  height * -0.02f,
                  width * 0.38f,
                  height * 0.18f,
                  {0.02f, 0.20f, 0.07f, 0.03f},
                  {0.0f, 0.10f, 0.03f, 0.0f},
                  32);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void renderScene(const AppState &app) {
  if (app.layout.width <= 0 || app.layout.height <= 0) {
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  configureProjection(app.layout);
  drawBackground(app);
  for (const RainLayer &layer : app.rainLayers) {
    drawColumnVeils(app, layer);
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  for (const RainLayer &layer : app.rainLayers) {
    drawRainGlyphs(app, layer, true);
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const RainLayer &layer : app.rainLayers) {
    drawRainGlyphs(app, layer, false);
  }
  drawForegroundEffects(app);
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
  auto *app = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (app == nullptr) {
    return;
  }

  updateLayout(*app, width, height);
  rebuildColumns(*app);
}

void keyCallback(GLFWwindow *window, int key, int, int action, int) {
  if (action != GLFW_PRESS) {
    return;
  }

  if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void printUsage(std::string_view executableName) {
  std::cout
      << "Usage: " << executableName
      << " [--fullscreen] [--width N --height N] [--seed N] [--sway] [--glyph-set NAME] [--charset TEXT]\n"
      << "       " << executableName << " [--windowed]\n\n"
      << "Options:\n"
      << "  --fullscreen   Launch on the primary monitor in fullscreen mode\n"
      << "  --windowed     Launch in a resizable window (default)\n"
      << "  --width N      Window width in pixels for windowed mode\n"
      << "  --height N     Window height in pixels for windowed mode\n"
      << "  --seed N       Use a fixed random seed for repeatable visuals\n"
      << "  --sway         Enable horizontal sway for rain columns\n"
      << "  --glyph-set    Glyph preset: procedural, pseudo-katakana, techno, custom\n"
      << "  --charset      Use a custom ASCII charset; implies --glyph-set custom\n"
      << "  --help         Show this help\n\n"
      << "Controls:\n"
      << "  Esc / Q        Exit\n"
      << "  Left click     Exit\n\n"
      << "Windows screensaver note:\n"
      << "  /s can be treated as fullscreen in a future .scr wrapper.\n";
}

bool parseInteger(std::string_view text, int &value) {
  try {
    std::size_t parsed = 0;
    const int candidate = std::stoi(std::string(text), &parsed, 10);
    if (parsed != text.size()) {
      return false;
    }
    value = candidate;
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool parseUnsigned(std::string_view text, std::uint32_t &value) {
  try {
    std::size_t parsed = 0;
    const unsigned long candidate = std::stoul(std::string(text), &parsed, 10);
    if (parsed != text.size() || candidate > std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    value = static_cast<std::uint32_t>(candidate);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

Config parseArgs(int argc, char **argv) {
  Config config;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];

    if (argument == "--fullscreen" || argument == "/s") {
      config.fullscreen = true;
      continue;
    }

    if (argument == "--windowed") {
      config.fullscreen = false;
      continue;
    }

    if (argument == "--sway") {
      config.sway = true;
      continue;
    }

    if (argument == "--glyph-set") {
      if (index + 1 >= argc) {
        throw std::runtime_error("--glyph-set expects one of: procedural, pseudo-katakana, techno, custom");
      }

      const auto glyphSet = parseGlyphSetName(argv[index + 1]);
      if (!glyphSet.has_value()) {
        throw std::runtime_error("Unknown glyph set. Use: procedural, pseudo-katakana, techno, custom");
      }

      config.glyphSet = *glyphSet;
      ++index;
      continue;
    }

    if (argument == "--charset") {
      if (index + 1 >= argc || !hasVisibleCharsetSymbols(argv[index + 1])) {
        throw std::runtime_error("--charset expects a non-empty string with visible characters");
      }

      config.customCharset = argv[index + 1];
      config.glyphSet = GlyphSet::CustomCharset;
      ++index;
      continue;
    }

    if (argument == "--help" || argument == "-h") {
      config.showHelp = true;
      continue;
    }

    if (argument == "--width") {
      if (index + 1 >= argc || !parseInteger(argv[index + 1], config.width)) {
        throw std::runtime_error("--width expects a positive integer");
      }
      ++index;
      continue;
    }

    if (argument == "--height") {
      if (index + 1 >= argc || !parseInteger(argv[index + 1], config.height)) {
        throw std::runtime_error("--height expects a positive integer");
      }
      ++index;
      continue;
    }

    if (argument == "--seed") {
      std::uint32_t seed = 0;
      if (index + 1 >= argc || !parseUnsigned(argv[index + 1], seed)) {
        throw std::runtime_error("--seed expects an unsigned integer");
      }
      config.seed = seed;
      ++index;
      continue;
    }

    throw std::runtime_error("Unknown argument: " + std::string(argument));
  }

  if (config.width <= 0 || config.height <= 0) {
    throw std::runtime_error("Window size must be positive");
  }

  if (config.glyphSet == GlyphSet::CustomCharset && !hasVisibleCharsetSymbols(config.customCharset)) {
    throw std::runtime_error("--glyph-set custom requires --charset with at least one visible character");
  }

  return config;
}

}  // namespace

int main(int argc, char **argv) {
  Config config;
  try {
    config = parseArgs(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n\n";
    printUsage(argc > 0 ? argv[0] : "digital-rain-screensaver");
    return EXIT_FAILURE;
  }

  if (config.showHelp) {
    printUsage(argc > 0 ? argv[0] : "digital-rain-screensaver");
    return EXIT_SUCCESS;
  }

  glfwSetErrorCallback([](int code, const char *description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
  });

  if (glfwInit() != GLFW_TRUE) {
    std::cerr << "Failed to initialize GLFW.\n";
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_RESIZABLE, config.fullscreen ? GLFW_FALSE : GLFW_TRUE);

  GLFWmonitor *monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
  const GLFWvidmode *mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;

  const int windowWidth = (mode != nullptr) ? mode->width : config.width;
  const int windowHeight = (mode != nullptr) ? mode->height : config.height;

  GLFWwindow *window = glfwCreateWindow(windowWidth,
                                        windowHeight,
                                        "Digital Rain Screensaver",
                                        monitor,
                                        nullptr);
  if (window == nullptr) {
    std::cerr << "Failed to create GLFW window.\n";
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetInputMode(window, GLFW_CURSOR, config.fullscreen ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

  std::random_device randomDevice;
  const std::uint32_t seed = config.seed.value_or(randomDevice());

  AppState app {};
  app.config = config;
  app.glyphAtlas = buildGlyphAtlas(config);
  app.rainLayers = createRainLayers();
  app.rng = std::mt19937(seed);

  int framebufferWidth = 0;
  int framebufferHeight = 0;
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

  updateLayout(app, framebufferWidth, framebufferHeight);
  rebuildColumns(app);

  glfwSetWindowUserPointer(window, &app);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);

  double previousTime = glfwGetTime();

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();

    const double currentTime = glfwGetTime();
    const float deltaSeconds = clampDelta(currentTime - previousTime);
    previousTime = currentTime;
    app.timeSeconds = currentTime;

    updateColumns(app, deltaSeconds);
    renderScene(app);
    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
