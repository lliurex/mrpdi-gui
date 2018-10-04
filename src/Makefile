
APP_NAME=mrpdi-gui
GTKMM_COMPILE=`pkg-config gtkmm-3.0 --cflags`
GTKMM_LINK=`pkg-config gtkmm-3.0 --libs`
MRPDI_LINK=`pkg-config mrpdi --libs`
LIBCONFIG_COMPILE=`pkg-config libconfig++ --cflags`
LIBCONFIG_LINK=`pkg-config libconfig++ --libs`
LIBAPPINDICATOR_COMPILE=`pkg-config appindicator3-0.1 --cflags`
LIBAPPINDICATOR_LINK=`pkg-config appindicator3-0.1 --libs`

COMPILER_FLAGS=-I ../include/ $(LIBCONFIG_COMPILE) $(LIBAPPINDICATOR_COMPILE)
LINKER_FLAGS=$(GTKMM_LINK) $(MRPDI_LINK) $(LIBCONFIG_LINK) $(LIBAPPINDICATOR_LINK)

$(APP_NAME): mrpdi-gui.o
	g++  -o $(APP_NAME) mrpdi-gui.o $(LINKER_FLAGS)

all: $(APP_NAME)

mrpdi-gui.o: mrpdi-gui.cpp
	g++ $(COMPILER_FLAGS) -c mrpdi-gui.cpp $(GTKMM_COMPILE)

install: $(APP_NAME)
	cp $(APP_NAME) /usr/bin/
	mkdir -p /usr/share/mrpdi/gui/
	cp ../rsrc/*.glade /usr/share/mrpdi/gui/
	cp ../rsrc/*.svg /usr/share/mrpdi/gui/
	
clean:
	rm -f *.o
	rm -rf $(APP_NAME)
	

