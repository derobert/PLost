CPPFLAGS += -I../inc

all: PLostd PLost

PLostd: PLostd.o
	$(CXX) $(LDFLAFGS) -o "$@" "$<"
PLost: PLost.o
	$(CXX) $(LDFLAFGS) -o "$@" "$<"

PLostd.o: ../inc/PLost.h PLostd.cpp
PLost.o: ../inc/PLost.h PLost.cpp
