#include "vk_engine.hpp"

#include "msdfgen-ext.h"
#include "msdfgen.h"

auto main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) -> int {
  VulkanEngine engine;

  // msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
  // if (ft) {
  //   msdfgen::FontHandle *font =
  //       msdfgen::loadFont(ft, "C:\\Windows\\Fonts\\arialbd.ttf");
  //   if (font) {
  //     msdfgen::Shape shape;
  //     if (msdfgen::loadGlyph(shape, font, 'A')) {
  //       shape.normalize();
  //       // max. angle
  //       msdfgen::edgeColoringSimple(shape, 3.0);
  //       // image width, height
  //       msdfgen::Bitmap<float, 3> msdf(32, 32);
  //       // range, scale, translation
  //       msdfgen::generateMSDF(msdf, shape, 4.0, 1.0,
  //                             msdfgen::Vector2(4.0, 4.0));
  //       msdfgen::savePng(msdf, "output.png");
  //     }
  //     msdfgen::destroyFont(font);
  //   }
  //   msdfgen::deinitializeFreetype(ft);
  // }
  // return 0;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
