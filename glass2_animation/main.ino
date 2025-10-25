#include <M5UnitGLASS2.h>
#include <M5Unified.h>
// ※ 日本語フォントは使わないので SPIFFS は不要

#define M5UNITGLASS2_ADDR 0x3C

int index_unit_glass2;

constexpr uint16_t C_WHITE = 0xFFFF;
constexpr int W = 128;
constexpr int H = 64;

// -------- 共通 ----------
inline int clip(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 右→左マルキー（英字）
template <typename D>
void marqueeEN(D& d, const char* msg,
               int text_size=2, int speed_px=1, int frame_ms=14) {
  M5Canvas sp(&d);
  sp.setColorDepth(1);
  sp.createSprite(W, H);
  sp.setTextWrap(false);
  sp.setTextSize(text_size);
  sp.setTextColor(C_WHITE, 0);

  int tw = sp.textWidth(msg);
  int th = 8 * text_size;
  int y  = (H - th) / 2;

  for (int x = W; x + tw > 0; x -= speed_px) {
    sp.fillScreen(0);
    sp.setCursor(x, y);
    sp.print(msg);
    sp.pushSprite(0, 0);
    delay(frame_ms);
  }
  sp.deleteSprite();
}

// 中央表示（ホールド時間付き）
template <typename D>
void centerPrint(D& d, const String& msg, int text_size=2, int hold_ms=1200) {
  M5Canvas sp(&d);
  sp.setColorDepth(1);
  sp.createSprite(W, H);
  sp.fillScreen(0);
  sp.setTextWrap(false);
  sp.setTextSize(text_size);
  sp.setTextColor(C_WHITE, 0);

  int tw = sp.textWidth(msg);
  int th = 8 * text_size;
  int x  = (W - tw) / 2;
  int y  = (H - th) / 2;
  sp.setCursor(x, y);
  sp.print(msg);
  sp.pushSprite(0, 0);
  delay(hold_ms);
  sp.deleteSprite();
}

// -------- アニメーション（短め版） ----------
template <typename D>
void animRipple(D& d, int frames=80, int step=6, int frame_ms=18) {
  for (int phase = 0; phase < frames; ++phase) {
    d.clear();
    for (int i = 0; i < 6; ++i) {
      int r = (i * (H / 7) + phase * step) % H;
      d.drawCircle(W/2, H/2, r, C_WHITE);
    }
    delay(frame_ms);
  }
}

template <typename D>
void animLissajous(D& d, int frames=120, int frame_ms=12) {
  d.clear();
  float A = (W - 6) / 2.0f, B = (H - 6) / 2.0f;
  float a = 3.0f, b = 2.0f;
  int px = W/2, py = H/2;
  for (int t = 0; t < frames; ++t) {
    int x1 = (int)(W/2 + A * sinf(t * a * 0.07f));
    int y1 = (int)(H/2 + B * sinf(t * b * 0.07f));
    d.drawLine(px, py, x1, y1, C_WHITE);
    px = x1; py = y1;
    delay(frame_ms);
  }
}

template <typename D>
void animFlower(D& d, int frames=150, int frame_ms=12) {
  // 回転する放射線（短く）
  int cx = W/2, cy = H/2, r = (H/2) - 3;
  for (int k = 0; k < frames; ++k) {
    d.clear();
    for (int p = 0; p < 24; ++p) {
      float ang = (p * (360.0f / 24) + k*8) * (3.1415926f / 180.0f);
      int x = cx + (int)(r * cosf(ang));
      int y = cy + (int)(r * sinf(ang));
      d.drawLine(cx, cy, x, y, C_WHITE);
    }
    delay(frame_ms);
  }
}

template <typename D>
void animBox(D& d, int duration_ms=5000, int frame_ms=14) {
  int x=12, y=10, w=34, h=20, vx=2, vy=1;
  unsigned long t0 = millis();
  while ((int)(millis() - t0) < duration_ms) {
    d.clear();
    d.drawRect(x, y, w, h, C_WHITE);
    d.drawLine(x, y, x+w, y+h, C_WHITE);
    d.drawLine(x+w, y, x, y+h, C_WHITE);
    x += vx; y += vy;
    if (x <= 0 || x + w >= W) vx = -vx;
    if (y <= 0 || y + h >= H) vy = -vy;
    x = clip(x, 0, W-w); y = clip(y, 0, H-h);
    delay(frame_ms);
  }
}

template <typename D>
void playRandomShortAnim(D& d) {
  switch (random(4)) {
    case 0: animRipple(d);    break;
    case 1: animLissajous(d); break;
    case 2: animFlower(d);    break;
    case 3: animBox(d);       break;
  }
}

// -------- 結果（スロット風 → 確定 → 周囲エフェクト） ----------
template <typename D>
void showResultWithRollAndFX(D& d,
                             int roll_ms = 2200,       // スロットぐるぐる時間
                             int final_fx_ms = 2200,   // 確定後エフェクト時間
                             int final_hold_ms = 2500) // 最後に見せる時間
{
  static const char* pool[] = {"5", "100", "3141592", "2718281828"};
  const char* finalVal = pool[random(4)];

  // ==== 1) スロット風ロール（大きめ・ゆっくり） ====
  unsigned long t0 = millis();
  while ((int)(millis() - t0) < roll_ms) {
    const char* v = pool[random(4)];
    int sz = 1 + ((millis() / 180) % 3);  // 1~4 と軽い揺れ
    centerPrint(d, v, sz, /*hold_ms=*/140);
  }

  // ==== 2) フラッシュ（確定直前の演出） ====
  d.clear(); delay(60);
  centerPrint(d, finalVal, /*text_size=*/1, /*hold_ms=*/1000);
  d.clear(); delay(50);

  // ==== 3) 確定後エフェクト（放射バースト＋同心円＋スパーク） ====
  // 文字とエフェクトは同じスプライトに重ねて描画してから push します
  M5Canvas sp(&d);
  sp.setColorDepth(1);
  sp.createSprite(W, H);
  sp.setTextWrap(false);
  sp.setTextSize(4);
  sp.setTextColor(C_WHITE, 0);

  const String msg = finalVal;
  const int tw = sp.textWidth(msg);
  const int th = 8 * 4; // text_size=4
  const int tx = (W - tw) / 2;
  const int ty = (H - th) / 2;
  const int cx = W / 2;
  const int cy = H / 2;

  unsigned long t1 = millis();
  while ((int)(millis() - t1) < final_fx_ms) {
    sp.fillScreen(0);

    // --- 同心円（波紋） ---
    int elapsed = (int)(millis() - t1);
    int baseR = 6 + (elapsed / 60) % 20;
    for (int i = 0; i < 3; ++i) {
      int r = baseR + i * 6;
      sp.drawCircle(cx, cy, r, C_WHITE);
    }

    // --- 放射バースト（回転するレイ） ---
    float t = (millis() - t1) * 0.012f;      // 回転速度
    int rays = 12;
    int R = (H / 2) - 2;
    for (int i = 0; i < rays; ++i) {
      float ang = t + i * (2 * 3.1415926f / rays);
      int x = cx + (int)(R * cosf(ang));
      int y = cy + (int)(R * sinf(ang));
      sp.drawLine(cx, cy, x, y, C_WHITE);
    }

    // --- スパーク（文字の周りに点） ---
    // 文字の外周矩形を少し広げた範囲にランダムな点を打つ
    int bx0 = max(0, tx - 6), by0 = max(0, ty - 6);
    int bx1 = min(W - 1, tx + tw + 6), by1 = min(H - 1, ty + th + 6);
    for (int k = 0; k < 24; ++k) {
      int sx = random(bx0, bx1 + 1);
      int sy = random(by0, by1 + 1);
      // 周辺のみ（内側は抜く）— ボーダー感
      if (sx < tx - 2 || sx > tx + tw + 2 || sy < ty - 2 || sy > ty + th + 2) {
        sp.drawPixel(sx, sy, C_WHITE);
      }
    }

    // --- 文字（最後に重ねる：最前面） ---
    sp.setCursor(tx, ty);
    sp.print(msg);

    // 描画
    sp.pushSprite(0, 0);
    delay(18);
  }

  sp.deleteSprite();

  // ==== 4) 最終ホールド（シンプルにドン） ====
  centerPrint(d, finalVal, /*text_size=*/1, /*hold_ms=*/final_hold_ms);
}

// -------- セットアップ／メイン --------
void setup() {
  auto cfg = M5.config();
  cfg.external_display.unit_glass2 = true;
  M5.begin(cfg);

  index_unit_glass2 = M5.getDisplayIndex(m5::board_t::board_M5UnitGLASS2);

  // 乱数シード
  randomSeed((uint32_t)(esp_timer_get_time() ^ millis()));

  auto& d = M5.Displays(index_unit_glass2);
  d.clear();
  centerPrint(d, "GLASS2 Ready", 2, 600);
}

void loop() {
  auto& d = M5.Displays(index_unit_glass2);

  // 1) メッセージは右→左に流す（短め）
  marqueeEN(d, "Start", /*text_size=*/3, /*speed_px=*/1, /*frame_ms=*/12);

  // 2) 短いアニメーションをランダム再生
  playRandomShortAnim(d);

  // 3) スロット風 → 最終値を長めに表示
  showResultWithRollAndFX(d, /*roll_ms=*/5000, /*final_fx_ms=*/10000, /*final_hold_ms=*/3000);

  delay(1000);
}
