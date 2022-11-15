#INCLUDE=-I/usr/include/x86_64-linux-gnu/qt5 -I/usr/include/x86_64-linux-gnu/qt5/QtGui -I/usr/include/x86_64-linux-gnu/qt5/QtCore -I/usr/include/x86_64-linux-gnu/qt5/QtWidgets -I/usr/include/x86_64-linux-gnu/qt5/QtMultimedia
INCLUDE=-I/usr/include/qt -I/usr/include/qt/QtGui -I/usr/include/qt/QtCore -I/usr/include/qt/QtWidgets -I/usr/include/qt/QtMultimedia
#CC=clang -O2 -pthread
CC=clang -g -pthread
LINK=-lstdc++ -L/usr/lib/x86_64-linux-gnu/ -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Multimedia -lpng

random_walk_test: qt_display.o random_walk_test.cc
	${CC} ${INCLUDE} ${LINK} random_walk_test.cc qt_display.o -o random_walk_test

qt_display.o: qt_display.h qt_display.cc
	${CC} ${INCLUDE} -c qt_display.cc

clean:
	rm qt_display.o
