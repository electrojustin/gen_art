#include "qt_display.h"

#include <stdlib.h>

QtDisplay::QtDisplay(int width, int height) {
  this->width = width;
  this->height = height;

  framebuf = (uint8_t*)malloc(width*height*4);

  setFixedSize(width, height);
  setWindowTitle("test");
  show();

  needs_repaint = false;

  startTimer(5);
}

QtDisplay::~QtDisplay() {
  free(framebuf);
}

void QtDisplay::paintEvent(QPaintEvent* e) {
  Q_UNUSED(e);

  QPainter qp(this);

  framebuf_mutex.lock();
  QImage image(framebuf, width, height, width * 4, QImage::Format_RGB32);
  qp.drawPixmap(0, 0, width, height, QPixmap::fromImage(image));
  framebuf_mutex.unlock();

  needs_repaint = false;
}

void QtDisplay::timerEvent(QTimerEvent *e) {
  Q_UNUSED(e);

  if (needs_repaint)
    this->repaint();
}

void QtDisplay::swap_buf(uint8_t* new_framebuf) {
  framebuf_mutex.lock();
  memcpy(framebuf, new_framebuf, width*height*4);
  needs_repaint = true;
  framebuf_mutex.unlock();
}
