#include <M5UnitGLASS2.h>
#include <M5Unified.h>

#define M5UNITGLASS2_ADDR 0x3C  // For Unit Glass2, 0x3C is default, and 0x3D is modified.

int index_unit_glass2;

void setup() {
  auto cfg = M5.config();
  cfg.external_display.unit_glass2 = true;
  M5.begin(cfg);

  index_unit_glass2 = M5.getDisplayIndex(m5::board_t::board_M5UnitGLASS2);
  M5.Displays(index_unit_glass2).setTextSize(2);
  M5.Displays(index_unit_glass2).clear();
}

void loop() {
  M5.Displays(index_unit_glass2).setCursor(0, 0);
  M5.Displays(index_unit_glass2).print("This is \nUnit \nGlass2");
  delay(1000);
  M5.Displays(index_unit_glass2).clear();

  M5.Displays(index_unit_glass2).drawRect(0, 0, 128, 64, 0xFFFF);  // x, y, w, h, color
  delay(500);
  M5.Displays(index_unit_glass2).drawCircle(20, 20, 15, 0xFFFF);  // x, y, r, color
  delay(500);
  M5.Displays(index_unit_glass2).fillArc(50, 45, 17, 20, 100, 330, 0xFFFF);  // x, y, r0, r1, angle0, angle1, color
  delay(500);
  M5.Displays(index_unit_glass2).fillRect(45, 10, 25, 10, 0xFFFF);  // x, y, w, h, color
  delay(500);
  M5.Displays(index_unit_glass2).fillTriangle(90, 10, 80, 40, 110, 55, 0xFFFF);  // x0, y0, x1, y1, x2, y2, color
  delay(500);
  M5.Displays(index_unit_glass2).drawLine(110, 0, 128, 64, 0xFFFF);  // x0, y0, x1, y1, color
  delay(1000);

  M5.Displays(index_unit_glass2).clear();
} 
