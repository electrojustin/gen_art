#include <stdint.h>
#include <QPainter>
#include <QSoundEffect>
#include <QWidget>
#include <mutex>

#ifndef QT_DISPLAY_H
#define QT_DISPLAY_H

class QtDisplay : public QWidget {
private:
  int width;
  int height;

  std::mutex framebuf_mutex;
  uint8_t *framebuf;

  std::atomic<bool> needs_repaint;

protected:
  void paintEvent(QPaintEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

public:
  QtDisplay(int width, int height);
  ~QtDisplay();

  void swap_buf(uint8_t* new_framebuf);
};

#endif
