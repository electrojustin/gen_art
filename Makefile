INCLUDE=-I/usr/include/x86_64-linux-gnu/qt5 -I/usr/include/x86_64-linux-gnu/qt5/QtGui -I/usr/include/x86_64-linux-gnu/qt5/QtCore -I/usr/include/x86_64-linux-gnu/qt5/QtWidgets -I/usr/include/x86_64-linux-gnu/qt5/QtMultimedia
#INCLUDE=-I/usr/include/qt -I/usr/include/qt/QtGui -I/usr/include/qt/QtCore -I/usr/include/qt/QtWidgets -I/usr/include/qt/QtMultimedia
#CC=clang -O2 -pthread
CC=clang -g -pthread -fPIC
LINK=-lstdc++ -L/usr/lib/x86_64-linux-gnu/ -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Multimedia -lpng -lfftw3 -lm

all: random_walk_test lightning frequency_sweep
lightning: lightning.cc markov.o qt_display.o
	${CC} ${INCLUDE} ${LINK} lightning.cc markov.o qt_display.o -o lightning
markov.o: markov.h markov.cc
	${CC} ${INCLUDE} -c markov.cc
random_walk_test: qt_display.o random_walk_test.cc
	${CC} ${INCLUDE} ${LINK} random_walk_test.cc qt_display.o -o random_walk_test
frequency_sweep: qt_display.o frequency_sweep.cc
	${CC} ${INCLUDE} ${LINK} frequency_sweep.cc qt_display.o -o frequency_sweep
qt_display.o: qt_display.h qt_display.cc
	${CC} ${INCLUDE} -c qt_display.cc
clean:
	rm markov.o lightning random_walk_test frequency_sweep qt_display.o
