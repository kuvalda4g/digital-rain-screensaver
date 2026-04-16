#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <bit>
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

struct Color {
  float r {};
  float g {};
  float b {};
  float a {};
};

struct Config {
  bool fullscreen = false;
  int width = 1280;
  int height = 720;
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
  std::vector<std::uint8_t> glyphs;
  std::vector<float> mutationTimers;
};

struct AppState {
  Config config;
  Layout layout;
  std::vector<Glyph> glyphAtlas;
  std::vector<Column> columns;
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

std::vector<Glyph> buildGlyphAtlas() {
  std::vector<Glyph> atlas;
  atlas.reserve(kGlyphCount);
  for (int index = 0; index < kGlyphCount; ++index) {
    atlas.push_back(buildProceduralGlyph(index));
  }
  return atlas;
}

float clampDelta(double value) {
  return std::clamp(static_cast<float>(value), 0.0f, 0.05f);
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

void resetColumn(AppState &app, Column &column, bool staggeredStart) {
  const int rowCount = app.layout.rows;

  column.speed = randomFloat(app.rng, 10.0f, 28.0f);
  column.trailLength = randomInt(app.rng, std::max(10, rowCount / 6), std::max(12, rowCount / 2));
  column.respawnGap = randomFloat(app.rng, 2.0f, 14.0f);
  column.phase = randomFloat(app.rng, 0.0f, 2.0f * kPi);
  column.intensity = randomFloat(app.rng, 0.85f, 1.2f);
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
  if (app.layout.cols <= 0 || app.layout.rows <= 0) {
    app.columns.clear();
    return;
  }

  app.columns.assign(static_cast<std::size_t>(app.layout.cols), {});
  for (Column &column : app.columns) {
    resetColumn(app, column, true);
  }
}

void updateColumns(AppState &app, float deltaSeconds) {
  if (app.columns.empty()) {
    return;
  }

  const int rowCount = app.layout.rows;

  for (Column &column : app.columns) {
    column.head += column.speed * deltaSeconds;

    const int minRow = std::max(0, static_cast<int>(std::floor(column.head)) - column.trailLength - 1);
    const int maxRow = std::min(rowCount - 1, static_cast<int>(std::ceil(column.head)) + 1);

    for (int row = minRow; row <= maxRow; ++row) {
      auto &timer = column.mutationTimers[static_cast<std::size_t>(row)];
      timer -= deltaSeconds;

      if (timer <= 0.0f) {
        column.glyphs[static_cast<std::size_t>(row)] = randomGlyphIndex(app.rng, app.glyphAtlas.size());
        timer = randomFloat(app.rng, 0.04f, 0.28f);
      }
    }

    if (randomFloat(app.rng, 0.0f, 1.0f) < deltaSeconds * 8.0f) {
      const int row = randomInt(app.rng, 0, rowCount - 1);
      column.glyphs[static_cast<std::size_t>(row)] = randomGlyphIndex(app.rng, app.glyphAtlas.size());
      column.mutationTimers[static_cast<std::size_t>(row)] = randomFloat(app.rng, 0.04f, 0.24f);
    }

    if (column.head - static_cast<float>(column.trailLength) >
        static_cast<float>(rowCount) + column.respawnGap) {
      resetColumn(app, column, false);
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

void drawBackground(const Layout &layout) {
  glBegin(GL_QUADS);
  glColor4f(0.0f, 0.06f, 0.01f, 1.0f);
  glVertex2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(layout.width), 0.0f);
  glColor4f(0.0f, 0.02f, 0.0f, 1.0f);
  glVertex2f(static_cast<float>(layout.width), static_cast<float>(layout.height));
  glVertex2f(0.0f, static_cast<float>(layout.height));
  glEnd();
}

Color makeCoreColor(const Column &column, float intensity, float shimmer, bool isHead) {
  if (isHead) {
    return {0.82f, 1.0f, 0.90f, 0.98f};
  }

  const float alpha = std::clamp((0.12f + intensity * 0.88f) * column.intensity, 0.0f, 0.92f);
  const float green = std::clamp(0.28f + intensity * 0.70f + shimmer * 0.08f, 0.0f, 1.0f);
  const float red = std::clamp(0.015f + intensity * 0.10f, 0.0f, 0.3f);
  const float blue = std::clamp(0.04f + intensity * 0.12f, 0.0f, 0.24f);
  return {red, green, blue, alpha};
}

Color makeGlowColor(float intensity, bool isHead) {
  if (isHead) {
    return {0.18f, 0.95f, 0.28f, 0.25f};
  }

  return {0.0f, std::clamp(0.10f + intensity * 0.22f, 0.0f, 0.4f), 0.02f, intensity * 0.12f};
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
  drawBackground(app.layout);

  glBegin(GL_QUADS);

  for (std::size_t columnIndex = 0; columnIndex < app.columns.size(); ++columnIndex) {
    const Column &column = app.columns[columnIndex];
    const int startRow = std::max(0, static_cast<int>(std::floor(column.head)) - column.trailLength);
    const int endRow = std::min(app.layout.rows - 1, static_cast<int>(std::ceil(column.head)));

    for (int row = startRow; row <= endRow; ++row) {
      const float distanceFromHead = column.head - static_cast<float>(row);
      if (distanceFromHead < 0.0f || distanceFromHead > static_cast<float>(column.trailLength)) {
        continue;
      }

      const bool isHead = distanceFromHead < 0.85f;
      const float normalized = 1.0f - (distanceFromHead / static_cast<float>(column.trailLength));
      const float intensity = std::pow(std::max(normalized, 0.0f), 1.35f);
      const float shimmer =
          std::sin(static_cast<float>(app.timeSeconds) * 7.5f + column.phase + (static_cast<float>(row) * 0.55f));

      const Color glowColor = makeGlowColor(intensity, isHead);
      const Color coreColor = makeCoreColor(column, intensity, shimmer, isHead);
      const Glyph &glyph = app.glyphAtlas[column.glyphs[static_cast<std::size_t>(row)]];

      const float x = (static_cast<float>(columnIndex) * app.layout.cellWidth) + app.layout.glyphOffsetX;
      const float y = (static_cast<float>(row) * app.layout.cellHeight) + app.layout.glyphOffsetY;

      emitGlyph(glyph, x, y, app.layout.pixelSize, glowColor, app.layout.pixelSize * 0.75f);
      emitGlyph(glyph, x, y, app.layout.pixelSize, coreColor, 0.0f);
    }
  }

  glEnd();

  glBegin(GL_QUADS);
  for (float y = 0.0f; y < static_cast<float>(app.layout.height); y += app.layout.pixelSize * 4.0f) {
    emitRect(0.0f, y, static_cast<float>(app.layout.width), 1.0f, {0.0f, 0.0f, 0.0f, 0.035f});
  }
  glEnd();
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
      << "Usage: " << executableName << " [--fullscreen] [--width N --height N] [--seed N]\n"
      << "       " << executableName << " [--windowed]\n\n"
      << "Options:\n"
      << "  --fullscreen   Launch on the primary monitor in fullscreen mode\n"
      << "  --windowed     Launch in a resizable window (default)\n"
      << "  --width N      Window width in pixels for windowed mode\n"
      << "  --height N     Window height in pixels for windowed mode\n"
      << "  --seed N       Use a fixed random seed for repeatable visuals\n"
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
  app.glyphAtlas = buildGlyphAtlas();
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
