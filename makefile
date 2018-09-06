
CXX = g++

SRC_DIR = ./src/
TARGET = duck_db
OBJ = bpt.o duck_db.o

$(TARGET):$(OBJ)
	$(CXX) -o $(TARGET) $(OBJ)
	rm -rf $(OBJ)

bpt.o:
	$(CXX) -c $(SRC_DIR)bpt.cc

duck_db.o:
	$(CXX) -c $(SRC_DIR)duck_db.cpp

clean:
	rm -rf $(OBJ) $(TARGET)