#include <GLFW/glfw3.h>

#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <shellapi.h>
#endif

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

enum class LaunchMode {
  Standard,
  ScreensaverFullscreen,
  ScreensaverPreview,
  ConfigurationDialog,
  PasswordChange,
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

struct LaunchOptions {
  Config config;
  LaunchMode mode = LaunchMode::Standard;
  std::uintptr_t nativeParentWindow = 0;
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
  bool closeOnAnyKey = false;
  bool closeOnMouseButton = true;
  bool closeOnMouseMove = false;
  bool previewMode = false;
  bool mouseBaselineCaptured = false;
  double mouseBaselineX = 0.0;
  double mouseBaselineY = 0.0;
  double mouseMoveThreshold = 8.0;
#ifdef _WIN32
  std::uintptr_t previewParentWindow = 0;
#endif
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

#ifdef _WIN32
std::string_view glyphSetToConfigName(GlyphSet glyphSet) {
  switch (glyphSet) {
    case GlyphSet::Procedural:
      return "procedural";
    case GlyphSet::PseudoKatakana:
      return "pseudo-katakana";
    case GlyphSet::Techno:
      return "techno";
    case GlyphSet::CustomCharset:
      return "custom";
  }

  return "procedural";
}
#endif

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

bool parseUnsigned(std::string_view text, std::uint32_t &value);

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
  auto *app = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (app == nullptr) {
    return;
  }

  if (action != GLFW_PRESS) {
    return;
  }

  if (app->closeOnAnyKey) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }

  if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  auto *app = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (app == nullptr || !app->closeOnMouseButton) {
    return;
  }

  if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void cursorPositionCallback(GLFWwindow *window, double x, double y) {
  auto *app = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (app == nullptr || !app->closeOnMouseMove) {
    return;
  }

  if (!app->mouseBaselineCaptured) {
    app->mouseBaselineCaptured = true;
    app->mouseBaselineX = x;
    app->mouseBaselineY = y;
    return;
  }

  if (std::abs(x - app->mouseBaselineX) >= app->mouseMoveThreshold ||
      std::abs(y - app->mouseBaselineY) >= app->mouseMoveThreshold) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

#ifdef _WIN32
bool tryParseNativeWindowHandle(std::string_view text, std::uintptr_t &value) {
  try {
    std::size_t parsed = 0;
    const auto candidate = std::stoull(std::string(text), &parsed, 0);
    if (parsed != text.size()) {
      return false;
    }

    value = static_cast<std::uintptr_t>(candidate);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

std::string utf8FromWide(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }

  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      text.data(),
                      static_cast<int>(text.size()),
                      result.data(),
                      size,
                      nullptr,
                      nullptr);
  return result;
}

std::vector<std::string> getWindowsCommandLineArgs() {
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return {"digital-rain-screensaver.scr"};
  }

  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));

  for (int index = 0; index < argc; ++index) {
    args.push_back(utf8FromWide(argv[index]));
  }

  LocalFree(argv);
  return args;
}

bool isValidParentWindow(std::uintptr_t nativeParentWindow) {
  return nativeParentWindow != 0 && IsWindow(reinterpret_cast<HWND>(nativeParentWindow)) != FALSE;
}

bool getParentClientSize(std::uintptr_t nativeParentWindow, int &width, int &height) {
  if (!isValidParentWindow(nativeParentWindow)) {
    return false;
  }

  RECT clientRect {};
  if (GetClientRect(reinterpret_cast<HWND>(nativeParentWindow), &clientRect) == FALSE) {
    return false;
  }

  width = std::max(1L, clientRect.right - clientRect.left);
  height = std::max(1L, clientRect.bottom - clientRect.top);
  return true;
}

bool attachPreviewWindow(GLFWwindow *window, std::uintptr_t nativeParentWindow) {
  if (!isValidParentWindow(nativeParentWindow)) {
    return false;
  }

  const HWND parentWindow = reinterpret_cast<HWND>(nativeParentWindow);
  const HWND childWindow = glfwGetWin32Window(window);
  if (childWindow == nullptr) {
    return false;
  }

  SetParent(childWindow, parentWindow);

  LONG_PTR style = GetWindowLongPtrW(childWindow, GWL_STYLE);
  style &= ~static_cast<LONG_PTR>(WS_POPUP);
  style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
  style |= static_cast<LONG_PTR>(WS_CHILD | WS_VISIBLE);
  SetWindowLongPtrW(childWindow, GWL_STYLE, style);

  LONG_PTR exStyle = GetWindowLongPtrW(childWindow, GWL_EXSTYLE);
  exStyle &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
  SetWindowLongPtrW(childWindow, GWL_EXSTYLE, exStyle);

  int width = 1;
  int height = 1;
  if (!getParentClientSize(nativeParentWindow, width, height)) {
    return false;
  }

  SetWindowPos(childWindow,
               HWND_TOP,
               0,
               0,
               width,
               height,
               SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  return true;
}

void syncPreviewWindow(GLFWwindow *window, std::uintptr_t nativeParentWindow) {
  if (!isValidParentWindow(nativeParentWindow)) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }

  int width = 1;
  int height = 1;
  if (!getParentClientSize(nativeParentWindow, width, height)) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }

  SetWindowPos(glfwGetWin32Window(window), HWND_TOP, 0, 0, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

int showWindowsMessage(std::string_view title, const std::string &message, std::uintptr_t nativeParentWindow) {
  const HWND owner = isValidParentWindow(nativeParentWindow) ? reinterpret_cast<HWND>(nativeParentWindow) : nullptr;
  MessageBoxA(owner, message.c_str(), std::string(title).c_str(), MB_OK | MB_ICONINFORMATION);
  return EXIT_SUCCESS;
}

std::wstring wideFromUtf8(std::string_view text) {
  if (text.empty()) {
    return {};
  }

  const int size =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }

  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8,
                      0,
                      text.data(),
                      static_cast<int>(text.size()),
                      result.data(),
                      size);
  return result;
}

std::wstring getEnvironmentVariableWide(const wchar_t *name) {
  const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
  if (required == 0) {
    return {};
  }

  std::wstring buffer(static_cast<std::size_t>(required), L'\0');
  const DWORD written = GetEnvironmentVariableW(name, buffer.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }

  buffer.resize(static_cast<std::size_t>(written));
  return buffer;
}

std::wstring readProfileString(std::wstring_view path,
                               const wchar_t *section,
                               const wchar_t *key,
                               std::wstring_view defaultValue) {
  std::wstring buffer(2048, L'\0');
  const DWORD written = GetPrivateProfileStringW(section,
                                                 key,
                                                 defaultValue.empty() ? L"" : defaultValue.data(),
                                                 buffer.data(),
                                                 static_cast<DWORD>(buffer.size()),
                                                 std::wstring(path).c_str());
  buffer.resize(static_cast<std::size_t>(written));
  return buffer;
}

bool ensureWindowsSettingsDirectory(std::wstring &directoryPath, std::string &errorMessage) {
  std::wstring basePath = getEnvironmentVariableWide(L"APPDATA");
  if (basePath.empty()) {
    errorMessage = "APPDATA is not available, so the Windows settings directory could not be resolved.";
    return false;
  }

  directoryPath = basePath + L"\\DigitalRainScreensaver";
  if (CreateDirectoryW(directoryPath.c_str(), nullptr) == FALSE) {
    const DWORD error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS) {
      errorMessage = "Failed to create the Windows settings directory.";
      return false;
    }
  }

  return true;
}

std::optional<std::wstring> getWindowsSettingsFilePath(std::string &errorMessage) {
  std::wstring directoryPath;
  if (!ensureWindowsSettingsDirectory(directoryPath, errorMessage)) {
    return std::nullopt;
  }

  return directoryPath + L"\\settings.ini";
}

Config makeSanitizedPersistentConfig(Config config) {
  config.showHelp = false;
  config.fullscreen = false;
  config.width = 1280;
  config.height = 720;

  if (config.glyphSet == GlyphSet::CustomCharset && !hasVisibleCharsetSymbols(config.customCharset)) {
    config.glyphSet = GlyphSet::Procedural;
    config.customCharset.clear();
  }

  return config;
}

Config loadPersistedConfig() {
  Config config;
  std::string errorMessage;
  const auto settingsPath = getWindowsSettingsFilePath(errorMessage);
  if (!settingsPath.has_value()) {
    return config;
  }

  config.sway = GetPrivateProfileIntW(L"visual", L"sway", config.sway ? 1 : 0, settingsPath->c_str()) != 0;

  const std::string glyphSetText = utf8FromWide(readProfileString(*settingsPath, L"visual", L"glyph_set", L"procedural"));
  if (const auto glyphSet = parseGlyphSetName(glyphSetText); glyphSet.has_value()) {
    config.glyphSet = *glyphSet;
  }

  config.customCharset = utf8FromWide(readProfileString(*settingsPath, L"visual", L"charset", L""));

  if (GetPrivateProfileIntW(L"visual", L"seed_enabled", 0, settingsPath->c_str()) != 0) {
    const std::string seedText = utf8FromWide(readProfileString(*settingsPath, L"visual", L"seed", L""));
    std::uint32_t seed = 0;
    if (parseUnsigned(seedText, seed)) {
      config.seed = seed;
    }
  }

  return makeSanitizedPersistentConfig(config);
}

bool savePersistedConfig(const Config &config, std::string &errorMessage) {
  const auto sanitized = makeSanitizedPersistentConfig(config);
  const auto settingsPath = getWindowsSettingsFilePath(errorMessage);
  if (!settingsPath.has_value()) {
    return false;
  }

  const std::wstring glyphSet = wideFromUtf8(glyphSetToConfigName(sanitized.glyphSet));
  const std::wstring charset = wideFromUtf8(sanitized.customCharset);
  const std::wstring seedText = sanitized.seed.has_value() ? wideFromUtf8(std::to_string(*sanitized.seed)) : L"";

  const bool ok =
      WritePrivateProfileStringW(L"visual", L"glyph_set", glyphSet.c_str(), settingsPath->c_str()) != FALSE &&
      WritePrivateProfileStringW(L"visual", L"charset", charset.c_str(), settingsPath->c_str()) != FALSE &&
      WritePrivateProfileStringW(L"visual", L"sway", sanitized.sway ? L"1" : L"0", settingsPath->c_str()) != FALSE &&
      WritePrivateProfileStringW(L"visual", L"seed_enabled", sanitized.seed.has_value() ? L"1" : L"0", settingsPath->c_str()) != FALSE &&
      WritePrivateProfileStringW(L"visual", L"seed", seedText.c_str(), settingsPath->c_str()) != FALSE;

  if (!ok) {
    errorMessage = "Failed to save the Windows screensaver settings.";
  }

  return ok;
}

constexpr wchar_t kConfigWindowClassName[] = L"DigitalRainScreensaverConfigWindow";

enum : int {
  kControlGlyphSetCombo = 1001,
  kControlCharsetEdit = 1002,
  kControlSwayCheckbox = 1003,
  kControlUseSeedCheckbox = 1004,
  kControlSeedEdit = 1005,
  kControlOkButton = 1006,
  kControlCancelButton = 1007,
  kControlDefaultsButton = 1008,
};

struct WindowsConfigDialogState {
  Config currentConfig;
  Config defaults;
  std::uintptr_t ownerWindow = 0;
  HWND window = nullptr;
  HWND glyphSetCombo = nullptr;
  HWND customCharsetLabel = nullptr;
  HWND customCharsetEdit = nullptr;
  HWND swayCheckbox = nullptr;
  HWND useSeedCheckbox = nullptr;
  HWND seedLabel = nullptr;
  HWND seedEdit = nullptr;
};

std::wstring_view glyphSetDisplayNameWide(GlyphSet glyphSet) {
  switch (glyphSet) {
    case GlyphSet::Procedural:
      return L"Procedural";
    case GlyphSet::PseudoKatakana:
      return L"Pseudo-katakana";
    case GlyphSet::Techno:
      return L"Techno";
    case GlyphSet::CustomCharset:
      return L"Custom charset";
  }

  return L"Procedural";
}

int glyphSetComboIndex(GlyphSet glyphSet) {
  switch (glyphSet) {
    case GlyphSet::Procedural:
      return 0;
    case GlyphSet::PseudoKatakana:
      return 1;
    case GlyphSet::Techno:
      return 2;
    case GlyphSet::CustomCharset:
      return 3;
  }

  return 0;
}

GlyphSet glyphSetFromComboIndex(int index) {
  switch (index) {
    case 1:
      return GlyphSet::PseudoKatakana;
    case 2:
      return GlyphSet::Techno;
    case 3:
      return GlyphSet::CustomCharset;
    case 0:
    default:
      return GlyphSet::Procedural;
  }
}

void setCheckboxState(HWND window, bool value) {
  SendMessageW(window, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool checkboxState(HWND window) {
  return SendMessageW(window, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void applyDefaultGuiFont(HWND window) {
  SendMessageW(window,
               WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               TRUE);
}

void setControlTextUtf8(HWND window, std::string_view text) {
  const std::wstring wideText = wideFromUtf8(text);
  SetWindowTextW(window, wideText.c_str());
}

std::string getControlTextUtf8(HWND window) {
  const int length = GetWindowTextLengthW(window);
  if (length <= 0) {
    return {};
  }

  std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
  GetWindowTextW(window, buffer.data(), length + 1);
  buffer.resize(static_cast<std::size_t>(length));
  return utf8FromWide(buffer);
}

void syncConfigDialogEnabledState(WindowsConfigDialogState &state) {
  const auto selectedGlyphSet =
      glyphSetFromComboIndex(static_cast<int>(SendMessageW(state.glyphSetCombo, CB_GETCURSEL, 0, 0)));
  const bool customCharset = selectedGlyphSet == GlyphSet::CustomCharset;
  EnableWindow(state.customCharsetLabel, customCharset ? TRUE : FALSE);
  EnableWindow(state.customCharsetEdit, customCharset ? TRUE : FALSE);

  const bool useSeed = checkboxState(state.useSeedCheckbox);
  EnableWindow(state.seedLabel, useSeed ? TRUE : FALSE);
  EnableWindow(state.seedEdit, useSeed ? TRUE : FALSE);
}

void applyConfigToDialog(WindowsConfigDialogState &state, const Config &config) {
  SendMessageW(state.glyphSetCombo, CB_SETCURSEL, glyphSetComboIndex(config.glyphSet), 0);
  setControlTextUtf8(state.customCharsetEdit, config.customCharset);
  setCheckboxState(state.swayCheckbox, config.sway);
  setCheckboxState(state.useSeedCheckbox, config.seed.has_value());
  setControlTextUtf8(state.seedEdit, config.seed.has_value() ? std::to_string(*config.seed) : "");
  syncConfigDialogEnabledState(state);
}

bool readConfigFromDialog(WindowsConfigDialogState &state, Config &config, std::string &errorMessage) {
  config = state.currentConfig;
  config.sway = checkboxState(state.swayCheckbox);
  config.glyphSet =
      glyphSetFromComboIndex(static_cast<int>(SendMessageW(state.glyphSetCombo, CB_GETCURSEL, 0, 0)));
  config.customCharset = getControlTextUtf8(state.customCharsetEdit);

  if (config.glyphSet == GlyphSet::CustomCharset && !hasVisibleCharsetSymbols(config.customCharset)) {
    errorMessage = "Custom glyph mode requires at least one visible character in the charset field.";
    return false;
  }

  if (checkboxState(state.useSeedCheckbox)) {
    const std::string seedText = getControlTextUtf8(state.seedEdit);
    std::uint32_t seed = 0;
    if (seedText.empty() || !parseUnsigned(seedText, seed)) {
      errorMessage = "The fixed seed must be a valid unsigned integer.";
      return false;
    }

    config.seed = seed;
  } else {
    config.seed.reset();
  }

  config.showHelp = false;
  return true;
}

void centerWindowRelativeToOwner(HWND window, std::uintptr_t nativeOwnerWindow) {
  RECT dialogRect {};
  GetWindowRect(window, &dialogRect);

  RECT anchorRect {};
  if (isValidParentWindow(nativeOwnerWindow)) {
    GetWindowRect(reinterpret_cast<HWND>(nativeOwnerWindow), &anchorRect);
  } else {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchorRect, 0);
  }

  const int dialogWidth = dialogRect.right - dialogRect.left;
  const int dialogHeight = dialogRect.bottom - dialogRect.top;
  const int anchorWidth = anchorRect.right - anchorRect.left;
  const int anchorHeight = anchorRect.bottom - anchorRect.top;

  const int x = anchorRect.left + std::max(0, (anchorWidth - dialogWidth) / 2);
  const int y = anchorRect.top + std::max(0, (anchorHeight - dialogHeight) / 2);

  SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK windowsConfigDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto *state = reinterpret_cast<WindowsConfigDialogState *>(GetWindowLongPtrW(window, GWLP_USERDATA));

  switch (message) {
    case WM_NCCREATE: {
      auto *createStruct = reinterpret_cast<CREATESTRUCTW *>(lParam);
      auto *dialogState = reinterpret_cast<WindowsConfigDialogState *>(createStruct->lpCreateParams);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialogState));
      dialogState->window = window;
      return TRUE;
    }

    case WM_CREATE: {
      if (state == nullptr) {
        return -1;
      }

      constexpr int margin = 16;
      constexpr int labelWidth = 128;
      constexpr int controlLeft = 156;
      constexpr int controlWidth = 230;
      constexpr int rowHeight = 28;

      HWND glyphSetLabel = CreateWindowExW(0,
                                           L"STATIC",
                                           L"Glyph set:",
                                           WS_CHILD | WS_VISIBLE,
                                           margin,
                                           18,
                                           labelWidth,
                                           20,
                                           window,
                                           nullptr,
                                           nullptr,
                                           nullptr);
      applyDefaultGuiFont(glyphSetLabel);

      state->glyphSetCombo = CreateWindowExW(0,
                                             L"COMBOBOX",
                                             nullptr,
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                             controlLeft,
                                             14,
                                             controlWidth,
                                             240,
                                             window,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlGlyphSetCombo)),
                                             nullptr,
                                             nullptr);
      applyDefaultGuiFont(state->glyphSetCombo);
      SendMessageW(state->glyphSetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(glyphSetDisplayNameWide(GlyphSet::Procedural).data()));
      SendMessageW(state->glyphSetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(glyphSetDisplayNameWide(GlyphSet::PseudoKatakana).data()));
      SendMessageW(state->glyphSetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(glyphSetDisplayNameWide(GlyphSet::Techno).data()));
      SendMessageW(state->glyphSetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(glyphSetDisplayNameWide(GlyphSet::CustomCharset).data()));

      state->customCharsetLabel = CreateWindowExW(0,
                                                  L"STATIC",
                                                  L"Custom charset:",
                                                  WS_CHILD | WS_VISIBLE,
                                                  margin,
                                                  18 + rowHeight + 10,
                                                  labelWidth,
                                                  20,
                                                  window,
                                                  nullptr,
                                                  nullptr,
                                                  nullptr);
      applyDefaultGuiFont(state->customCharsetLabel);

      state->customCharsetEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
                                                 L"EDIT",
                                                 nullptr,
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                                 controlLeft,
                                                 14 + rowHeight + 10,
                                                 controlWidth,
                                                 24,
                                                 window,
                                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlCharsetEdit)),
                                                 nullptr,
                                                 nullptr);
      applyDefaultGuiFont(state->customCharsetEdit);

      state->swayCheckbox = CreateWindowExW(0,
                                            L"BUTTON",
                                            L"Enable horizontal sway",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                            margin,
                                            18 + (rowHeight * 2) + 14,
                                            220,
                                            22,
                                            window,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlSwayCheckbox)),
                                            nullptr,
                                            nullptr);
      applyDefaultGuiFont(state->swayCheckbox);

      state->useSeedCheckbox = CreateWindowExW(0,
                                               L"BUTTON",
                                               L"Use fixed seed",
                                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                               margin,
                                               18 + (rowHeight * 3) + 18,
                                               220,
                                               22,
                                               window,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlUseSeedCheckbox)),
                                               nullptr,
                                               nullptr);
      applyDefaultGuiFont(state->useSeedCheckbox);

      state->seedLabel = CreateWindowExW(0,
                                         L"STATIC",
                                         L"Seed value:",
                                         WS_CHILD | WS_VISIBLE,
                                         margin,
                                         18 + (rowHeight * 4) + 20,
                                         labelWidth,
                                         20,
                                         window,
                                         nullptr,
                                         nullptr,
                                         nullptr);
      applyDefaultGuiFont(state->seedLabel);

      state->seedEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
                                        L"EDIT",
                                        nullptr,
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                                        controlLeft,
                                        14 + (rowHeight * 4) + 20,
                                        120,
                                        24,
                                        window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlSeedEdit)),
                                        nullptr,
                                        nullptr);
      applyDefaultGuiFont(state->seedEdit);

      HWND noteLabel = CreateWindowExW(0,
                                       L"STATIC",
                                       L"Saved settings apply to /s, /p, and regular Windows launches.",
                                       WS_CHILD | WS_VISIBLE,
                                       margin,
                                       18 + (rowHeight * 5) + 22,
                                       360,
                                       20,
                                       window,
                                       nullptr,
                                       nullptr,
                                       nullptr);
      applyDefaultGuiFont(noteLabel);

      HWND defaultsButton = CreateWindowExW(0,
                                            L"BUTTON",
                                            L"Defaults",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                            88,
                                            222,
                                            86,
                                            28,
                                            window,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlDefaultsButton)),
                                            nullptr,
                                            nullptr);
      applyDefaultGuiFont(defaultsButton);

      HWND cancelButton = CreateWindowExW(0,
                                          L"BUTTON",
                                          L"Cancel",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                          182,
                                          222,
                                          86,
                                          28,
                                          window,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlCancelButton)),
                                          nullptr,
                                          nullptr);
      applyDefaultGuiFont(cancelButton);

      HWND okButton = CreateWindowExW(0,
                                      L"BUTTON",
                                      L"Save",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                      276,
                                      222,
                                      110,
                                      28,
                                      window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlOkButton)),
                                      nullptr,
                                      nullptr);
      applyDefaultGuiFont(okButton);

      applyConfigToDialog(*state, state->currentConfig);
      centerWindowRelativeToOwner(window, state->ownerWindow);
      return 0;
    }

    case WM_COMMAND: {
      if (state == nullptr) {
        return 0;
      }

      const int controlId = LOWORD(wParam);
      const int notificationCode = HIWORD(wParam);

      if (controlId == kControlGlyphSetCombo && notificationCode == CBN_SELCHANGE) {
        syncConfigDialogEnabledState(*state);
        return 0;
      }

      if (controlId == kControlUseSeedCheckbox && notificationCode == BN_CLICKED) {
        syncConfigDialogEnabledState(*state);
        return 0;
      }

      if (controlId == kControlDefaultsButton && notificationCode == BN_CLICKED) {
        state->currentConfig = state->defaults;
        applyConfigToDialog(*state, state->currentConfig);
        return 0;
      }

      if (controlId == kControlCancelButton && notificationCode == BN_CLICKED) {
        DestroyWindow(window);
        return 0;
      }

      if (controlId == kControlOkButton && notificationCode == BN_CLICKED) {
        Config updatedConfig;
        std::string errorMessage;
        if (!readConfigFromDialog(*state, updatedConfig, errorMessage)) {
          showWindowsMessage("Digital Rain Screensaver", errorMessage, state->ownerWindow);
          return 0;
        }

        if (!savePersistedConfig(updatedConfig, errorMessage)) {
          showWindowsMessage("Digital Rain Screensaver", errorMessage, state->ownerWindow);
          return 0;
        }

        state->currentConfig = updatedConfig;
        DestroyWindow(window);
        return 0;
      }

      return 0;
    }

    case WM_CLOSE:
      DestroyWindow(window);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProcW(window, message, wParam, lParam);
  }
}

bool ensureConfigWindowClassRegistered(HINSTANCE instance, std::string &errorMessage) {
  WNDCLASSEXW windowClass {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = windowsConfigDialogProc;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  windowClass.lpszClassName = kConfigWindowClassName;

  if (RegisterClassExW(&windowClass) == 0) {
    const DWORD error = GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      errorMessage = "Failed to register the Windows configuration window class.";
      return false;
    }
  }

  return true;
}

int runWindowsConfigurationDialog(const Config &initialConfig, std::uintptr_t nativeOwnerWindow) {
  std::string errorMessage;
  if (!ensureConfigWindowClassRegistered(GetModuleHandleW(nullptr), errorMessage)) {
    return showWindowsMessage("Digital Rain Screensaver", errorMessage, nativeOwnerWindow);
  }

  WindowsConfigDialogState dialogState {
      .currentConfig = makeSanitizedPersistentConfig(initialConfig),
      .defaults = Config {},
      .ownerWindow = nativeOwnerWindow,
  };

  HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                kConfigWindowClassName,
                                L"Digital Rain Screensaver Settings",
                                WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                420,
                                300,
                                isValidParentWindow(nativeOwnerWindow) ? reinterpret_cast<HWND>(nativeOwnerWindow) : nullptr,
                                nullptr,
                                GetModuleHandleW(nullptr),
                                &dialogState);
  if (window == nullptr) {
    return showWindowsMessage("Digital Rain Screensaver",
                              "Failed to create the Windows configuration window.",
                              nativeOwnerWindow);
  }

  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  MSG message {};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (dialogState.window != nullptr && IsDialogMessageW(dialogState.window, &message) != FALSE) {
      continue;
    }

    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return EXIT_SUCCESS;
}
#endif

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
      << "Windows screensaver switches:\n"
      << "  /s             Run as a fullscreen screensaver\n"
      << "  /c             Open the native configuration window and save Windows defaults\n"
      << "  /p HWND        Run inside a preview parent window\n"
      << "\nOn Windows, settings saved through /c are used as the default visual profile.\n";
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

std::vector<std::string> collectArgs(int argc, char **argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(std::max(argc, 0)));

  for (int index = 0; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  if (args.empty()) {
    args.emplace_back("digital-rain-screensaver");
  }

  return args;
}

Config parseArgs(const std::vector<std::string> &args, Config baseConfig = {}) {
  Config config = baseConfig;
  config.showHelp = false;

  for (std::size_t index = 1; index < args.size(); ++index) {
    const std::string_view argument = args[index];

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
      if (index + 1 >= args.size()) {
        throw std::runtime_error("--glyph-set expects one of: procedural, pseudo-katakana, techno, custom");
      }

      const auto glyphSet = parseGlyphSetName(args[index + 1]);
      if (!glyphSet.has_value()) {
        throw std::runtime_error("Unknown glyph set. Use: procedural, pseudo-katakana, techno, custom");
      }

      config.glyphSet = *glyphSet;
      ++index;
      continue;
    }

    if (argument == "--charset") {
      if (index + 1 >= args.size() || !hasVisibleCharsetSymbols(args[index + 1])) {
        throw std::runtime_error("--charset expects a non-empty string with visible characters");
      }

      config.customCharset = args[index + 1];
      config.glyphSet = GlyphSet::CustomCharset;
      ++index;
      continue;
    }

    if (argument == "--help" || argument == "-h") {
      config.showHelp = true;
      continue;
    }

    if (argument == "--width") {
      if (index + 1 >= args.size() || !parseInteger(args[index + 1], config.width)) {
        throw std::runtime_error("--width expects a positive integer");
      }
      ++index;
      continue;
    }

    if (argument == "--height") {
      if (index + 1 >= args.size() || !parseInteger(args[index + 1], config.height)) {
        throw std::runtime_error("--height expects a positive integer");
      }
      ++index;
      continue;
    }

    if (argument == "--seed") {
      std::uint32_t seed = 0;
      if (index + 1 >= args.size() || !parseUnsigned(args[index + 1], seed)) {
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

LaunchOptions parseLaunchOptions(const std::vector<std::string> &args) {
  LaunchOptions options;
  std::vector<std::string> filteredArgs;
  filteredArgs.reserve(args.size());
  filteredArgs.push_back(args.empty() ? "digital-rain-screensaver" : args.front());

#ifdef _WIN32
  Config baseConfig = loadPersistedConfig();
  for (std::size_t index = 1; index < args.size(); ++index) {
    const std::string_view argument = args[index];
    if (argument.size() >= 2 && argument.front() == '/' && argument[1] != '/') {
      const char switchName = static_cast<char>(std::tolower(static_cast<unsigned char>(argument[1])));
      std::string_view inlineValue = argument.substr(2);
      while (!inlineValue.empty() &&
             (inlineValue.front() == ':' || inlineValue.front() == '=' ||
              std::isspace(static_cast<unsigned char>(inlineValue.front())) != 0)) {
        inlineValue.remove_prefix(1);
      }

      auto parseParentHandle = [&](bool required, std::string_view missingMessage, std::string_view invalidMessage) {
        std::uintptr_t handle = 0;
        if (!inlineValue.empty()) {
          if (!tryParseNativeWindowHandle(inlineValue, handle)) {
            throw std::runtime_error(std::string(invalidMessage));
          }
          options.nativeParentWindow = handle;
          return;
        }

        if (index + 1 < args.size()) {
          const std::string_view nextArgument = args[index + 1];
          if (tryParseNativeWindowHandle(nextArgument, handle)) {
            options.nativeParentWindow = handle;
            ++index;
            return;
          }
        }

        if (required) {
          throw std::runtime_error(std::string(missingMessage));
        }
      };

      switch (switchName) {
        case 's':
          if (inlineValue.empty()) {
            options.mode = LaunchMode::ScreensaverFullscreen;
            continue;
          }
          break;
        case 'c':
          options.mode = LaunchMode::ConfigurationDialog;
          parseParentHandle(false, "", "/c received an invalid parent window handle");
          continue;
        case 'p':
          options.mode = LaunchMode::ScreensaverPreview;
          parseParentHandle(true, "/p expects a parent preview window handle", "/p received an invalid parent window handle");
          continue;
        case 'a':
          options.mode = LaunchMode::PasswordChange;
          parseParentHandle(false, "", "/a received an invalid parent window handle");
          continue;
        default:
          break;
      }
    }

    filteredArgs.push_back(args[index]);
  }
#else
  Config baseConfig;
  filteredArgs = args;
#endif

  options.config = parseArgs(filteredArgs, baseConfig);

  if (options.mode == LaunchMode::ScreensaverFullscreen) {
    options.config.fullscreen = true;
  } else if (options.mode == LaunchMode::ScreensaverPreview) {
    options.config.fullscreen = false;
  }

  return options;
}

int runRenderer(const LaunchOptions &launchOptions) {
  glfwSetErrorCallback([](int code, const char *description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
  });

  if (glfwInit() != GLFW_TRUE) {
    std::cerr << "Failed to initialize GLFW.\n";
    return EXIT_FAILURE;
  }

  const bool previewMode = launchOptions.mode == LaunchMode::ScreensaverPreview;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_RESIZABLE, (!launchOptions.config.fullscreen && !previewMode) ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_DECORATED, previewMode ? GLFW_FALSE : GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, previewMode ? GLFW_FALSE : GLFW_TRUE);

  int windowWidth = launchOptions.config.width;
  int windowHeight = launchOptions.config.height;
  GLFWmonitor *monitor = nullptr;

#ifdef _WIN32
  if (previewMode) {
    if (!getParentClientSize(launchOptions.nativeParentWindow, windowWidth, windowHeight)) {
      std::cerr << "Preview mode requires a valid parent window handle.\n";
      glfwTerminate();
      return EXIT_FAILURE;
    }
  } else
#endif
  if (launchOptions.config.fullscreen) {
    monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
    if (mode != nullptr) {
      windowWidth = mode->width;
      windowHeight = mode->height;
    }
  }

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

#ifdef _WIN32
  if (previewMode && !attachPreviewWindow(window, launchOptions.nativeParentWindow)) {
    std::cerr << "Failed to attach preview window to the requested parent HWND.\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
#endif

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetInputMode(window,
                   GLFW_CURSOR,
                   launchOptions.config.fullscreen && !previewMode ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

  std::random_device randomDevice;
  const std::uint32_t seed = launchOptions.config.seed.value_or(randomDevice());

  AppState app {};
  app.config = launchOptions.config;
  app.glyphAtlas = buildGlyphAtlas(launchOptions.config);
  app.rainLayers = createRainLayers();
  app.rng = std::mt19937(seed);
  app.closeOnAnyKey = launchOptions.mode == LaunchMode::ScreensaverFullscreen;
  app.closeOnMouseButton = launchOptions.mode != LaunchMode::ScreensaverPreview;
  app.closeOnMouseMove = launchOptions.mode == LaunchMode::ScreensaverFullscreen;
  app.previewMode = previewMode;

#ifdef _WIN32
  app.previewParentWindow = launchOptions.nativeParentWindow;
#endif

  int framebufferWidth = 0;
  int framebufferHeight = 0;
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

  updateLayout(app, framebufferWidth, framebufferHeight);
  rebuildColumns(app);

  glfwSetWindowUserPointer(window, &app);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetCursorPosCallback(window, cursorPositionCallback);

  double previousTime = glfwGetTime();

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
#ifdef _WIN32
    if (previewMode) {
      syncPreviewWindow(window, launchOptions.nativeParentWindow);
    }
#endif

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

int runApp(const std::vector<std::string> &args) {
  const std::string executableName = args.empty() ? "digital-rain-screensaver" : args.front();

  LaunchOptions launchOptions;
  try {
    launchOptions = parseLaunchOptions(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n\n";
    printUsage(executableName);
    return EXIT_FAILURE;
  }

  if (launchOptions.config.showHelp) {
    printUsage(executableName);
    return EXIT_SUCCESS;
  }

#ifdef _WIN32
  if (launchOptions.mode == LaunchMode::ConfigurationDialog) {
    return runWindowsConfigurationDialog(launchOptions.config, launchOptions.nativeParentWindow);
  }

  if (launchOptions.mode == LaunchMode::PasswordChange) {
    std::string message =
        "Windows password-change screensaver hooks are not supported by this project.\n\n"
        "On modern Windows versions this path is typically unused.";
    return showWindowsMessage("Digital Rain Screensaver", message, launchOptions.nativeParentWindow);
  }
#endif

  return runRenderer(launchOptions);
}

}  // namespace

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  return runApp(getWindowsCommandLineArgs());
}
#else
int main(int argc, char **argv) {
  return runApp(collectArgs(argc, argv));
}
#endif
