CPPFLAGS += -I../inc

all: PLostd PLost

clean:
	rm -f PLostd PLost
	rm -f PLostd.o PLost.o

PLostd: PLostd.o
	$(CXX) $(LDFLAGS) -o "$@" "$<"
PLost: PLost.o
	$(CXX) $(LDFLAGS) -o "$@" "$<"

PLostd.o: ../inc/PLost.h PLostd.cpp
PLost.o: ../inc/PLost.h PLost.cpp
